#!/usr/bin/env bash
# Check the vendored liboqs SLH-DSA-SHA2-128s build against NIST ACVP vectors
# (src/test/data/slh_dsa_sha2_128s_acvp.json) and the Marscoin P2WPQH signing context.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OQS_INSTALL="${OQS_INSTALL:-$ROOT_DIR/src/crypto/oqs_vendor/build/install}"
VECTORS="$ROOT_DIR/src/test/data/slh_dsa_sha2_128s_acvp.json"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

if [[ ! -f "$OQS_INSTALL/include/oqs/oqs.h" ]] || [[ ! -f "$OQS_INSTALL/lib/liboqs.a" ]]; then
  echo "Missing vendored liboqs install artifacts under: $OQS_INSTALL" >&2
  echo "Run src/crypto/oqs_vendor/build-liboqs-vendor.sh first." >&2
  exit 1
fi

# Turn the JSON vectors into C++ initializers.
python3 - "$VECTORS" > "$WORK_DIR/vectors.h" <<'PY'
import json, sys
vectors = [v for v in json.load(open(sys.argv[1])) if isinstance(v, dict)]
def s(x): return '"' + x + '"'
print("static const KeyGenVector KEYGEN[] = {")
for v in vectors:
    if v["type"] == "keyGen":
        print("  {%d, %s, %s, %s, %s, %s}," % (v["tcId"], s(v["skSeed"]), s(v["skPrf"]), s(v["pkSeed"]), s(v["sk"]), s(v["pk"])))
print("};")
print("static const SigVerVector SIGVER[] = {")
for v in vectors:
    if v["type"] == "sigVer":
        print("  {%d, %s, %s, %s, %s, %s}," % (v["tcId"], s(v["pk"]), s(v["message"]), s(v["context"]), s(v["signature"]), "true" if v["testPassed"] else "false"))
print("};")
print("static const SigGenVector SIGGEN[] = {")
for v in vectors:
    if v["type"] == "sigGen":
        print("  {%d, %s, %s, %s, %s, %s, %s}," % (v["tcId"], "true" if v["deterministic"] else "false", s(v["sk"]), s(v["message"]), s(v["context"]), s(v["additionalRandomness"]), s(v["signature"])))
print("};")
PY

cat > "$WORK_DIR/kat.cpp" <<'CPP'
#include <oqs/oqs.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

struct KeyGenVector { int tc; const char* sk_seed; const char* sk_prf; const char* pk_seed; const char* sk; const char* pk; };
struct SigVerVector { int tc; const char* pk; const char* message; const char* context; const char* signature; bool passed; };
struct SigGenVector { int tc; bool deterministic; const char* sk; const char* message; const char* context; const char* addrnd; const char* signature; };
#include "vectors.h"

static std::vector<uint8_t> Hex(const char* hex)
{
    std::vector<uint8_t> out;
    for (size_t i = 0; hex[i] && hex[i + 1]; i += 2) out.push_back(static_cast<uint8_t>(std::stoul(std::string(hex + i, 2), nullptr, 16)));
    return out;
}

static std::vector<uint8_t> g_rng;
static size_t g_rng_pos = 0;
static bool g_rng_overrun = false;
static void ReplayRng(uint8_t* out, size_t len)
{
    if (g_rng_pos + len > g_rng.size()) { g_rng_overrun = true; std::memset(out, 0, len); return; }
    std::memcpy(out, g_rng.data() + g_rng_pos, len);
    g_rng_pos += len;
}
static void Replay(std::vector<uint8_t> bytes) { g_rng = std::move(bytes); g_rng_pos = 0; OQS_randombytes_custom_algorithm(ReplayRng); }

int main()
{
    OQS_SIG* sig = OQS_SIG_new(OQS_SIG_alg_slh_dsa_pure_sha2_128s);
    if (!sig || !sig->sig_with_ctx_support) { std::fprintf(stderr, "SLH-DSA-SHA2-128s with context support unavailable\n"); return 2; }
    int failures = 0;

    for (const auto& v : KEYGEN) {
        std::vector<uint8_t> seeds = Hex(v.sk_seed), prf = Hex(v.sk_prf), pk_seed = Hex(v.pk_seed);
        seeds.insert(seeds.end(), prf.begin(), prf.end());
        seeds.insert(seeds.end(), pk_seed.begin(), pk_seed.end());
        Replay(seeds);
        std::vector<uint8_t> pk(sig->length_public_key), sk(sig->length_secret_key);
        if (OQS_SIG_keypair(sig, pk.data(), sk.data()) != OQS_SUCCESS || pk != Hex(v.pk) || sk != Hex(v.sk)) {
            std::fprintf(stderr, "keyGen tcId %d failed\n", v.tc); ++failures;
        }
    }
    for (const auto& v : SIGVER) {
        const auto pk = Hex(v.pk), msg = Hex(v.message), ctx = Hex(v.context), s = Hex(v.signature);
        const bool ok = OQS_SIG_verify_with_ctx_str(sig, msg.data(), msg.size(), s.data(), s.size(), ctx.data(), ctx.size(), pk.data()) == OQS_SUCCESS;
        if (ok != v.passed) { std::fprintf(stderr, "sigVer tcId %d: got %d, expected %d\n", v.tc, ok, v.passed); ++failures; }
    }
    for (const auto& v : SIGGEN) {
        const auto sk = Hex(v.sk), msg = Hex(v.message), ctx = Hex(v.context);
        Replay(v.deterministic ? std::vector<uint8_t>(sk.begin() + 32, sk.begin() + 48) : Hex(v.addrnd));
        std::vector<uint8_t> s(sig->length_signature);
        size_t len = 0;
        if (OQS_SIG_sign_with_ctx_str(sig, s.data(), &len, msg.data(), msg.size(), ctx.data(), ctx.size(), sk.data()) != OQS_SUCCESS) {
            std::fprintf(stderr, "sigGen tcId %d: signing failed\n", v.tc); ++failures; continue;
        }
        s.resize(len);
        if (s != Hex(v.signature)) { std::fprintf(stderr, "sigGen tcId %d: signature mismatch\n", v.tc); ++failures; }
    }
    OQS_randombytes_switch_algorithm(OQS_RAND_alg_system);
    if (g_rng_overrun) { std::fprintf(stderr, "replayed randomness exhausted\n"); ++failures; }

    // Marscoin P2WPQH context: round trip, and rejection under other contexts.
    const char* ctx = "marscoin-p2wpqh-v1";
    const uint8_t msg[32] = {0x5a};
    std::vector<uint8_t> pk(sig->length_public_key), sk(sig->length_secret_key), s(sig->length_signature);
    size_t len = 0;
    if (OQS_SIG_keypair(sig, pk.data(), sk.data()) != OQS_SUCCESS ||
        OQS_SIG_sign_with_ctx_str(sig, s.data(), &len, msg, sizeof(msg), reinterpret_cast<const uint8_t*>(ctx), std::strlen(ctx), sk.data()) != OQS_SUCCESS ||
        OQS_SIG_verify_with_ctx_str(sig, msg, sizeof(msg), s.data(), len, reinterpret_cast<const uint8_t*>(ctx), std::strlen(ctx), pk.data()) != OQS_SUCCESS ||
        OQS_SIG_verify(sig, msg, sizeof(msg), s.data(), len, pk.data()) == OQS_SUCCESS) {
        std::fprintf(stderr, "Marscoin context round trip failed\n"); ++failures;
    }

    std::printf("keyGen=%zu sigVer=%zu sigGen=%zu failures=%d\n", sizeof(KEYGEN) / sizeof(KEYGEN[0]), sizeof(SIGVER) / sizeof(SIGVER[0]), sizeof(SIGGEN) / sizeof(SIGGEN[0]), failures);
    OQS_SIG_free(sig);
    return failures == 0 ? 0 : 1;
}
CPP

c++ -std=c++17 -O1 -I"$WORK_DIR" -I"$OQS_INSTALL/include" "$WORK_DIR/kat.cpp" "$OQS_INSTALL/lib/liboqs.a" -lpthread -o "$WORK_DIR/kat"
"$WORK_DIR/kat"
echo "OQS SLH-DSA KAT passed."
