#!/usr/bin/env bash
# Check the vendored SLH-DSA implementation (src/crypto/slhdsa, slhdsa-c) against
# the NIST ACVP vectors for SLH-DSA-SHA2-128s (src/test/data/slh_dsa_sha2_128s_acvp.json)
# and the Marscoin P2WPQH signing context. Compiles only the vendored sources and
# a small driver, so it needs no node build.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SLHDSA_DIR="$ROOT_DIR/src/crypto/slhdsa"
VECTORS="$ROOT_DIR/src/test/data/slh_dsa_sha2_128s_acvp.json"
CC="${CC:-cc}"
CXX="${CXX:-c++}"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

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
#include "slh_dsa.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

struct KeyGenVector { int tc; const char *sk_seed, *sk_prf, *pk_seed, *sk, *pk; };
struct SigVerVector { int tc; const char *pk, *message, *context, *signature; bool passed; };
struct SigGenVector { int tc; bool deterministic; const char *sk, *message, *context, *addrnd, *signature; };
#include "vectors.h"

static std::vector<uint8_t> Hex(const char* h)
{
    std::vector<uint8_t> out(std::strlen(h) / 2);
    for (size_t i = 0; i < out.size(); ++i) out[i] = static_cast<uint8_t>(std::stoi(std::string(h + 2 * i, 2), nullptr, 16));
    return out;
}

int main()
{
    const slh_param_t* prm = &slh_dsa_sha2_128s;
    int failures = 0;

    for (const auto& v : KEYGEN) {
        std::vector<uint8_t> pk(slh_pk_sz(prm)), sk(slh_sk_sz(prm));
        slh_keygen_internal(sk.data(), pk.data(), Hex(v.sk_seed).data(), Hex(v.sk_prf).data(), Hex(v.pk_seed).data(), prm);
        if (pk != Hex(v.pk) || sk != Hex(v.sk)) { std::fprintf(stderr, "keyGen tcId %d: key mismatch\n", v.tc); ++failures; }
    }
    for (const auto& v : SIGVER) {
        const auto pk = Hex(v.pk), m = Hex(v.message), ctx = Hex(v.context), s = Hex(v.signature);
        const bool ok = slh_verify(m.data(), m.size(), s.data(), s.size(), ctx.data(), ctx.size(), pk.data(), prm) == 1;
        if (ok != v.passed) { std::fprintf(stderr, "sigVer tcId %d: expected %d\n", v.tc, v.passed); ++failures; }
    }
    for (const auto& v : SIGGEN) {
        const auto sk = Hex(v.sk), m = Hex(v.message), ctx = Hex(v.context), addrnd = Hex(v.addrnd);
        std::vector<uint8_t> s(slh_sig_sz(prm));
        const size_t len = slh_sign(s.data(), m.data(), m.size(), ctx.data(), ctx.size(), sk.data(),
                                    v.deterministic ? nullptr : addrnd.data(), prm);
        s.resize(len);
        if (s != Hex(v.signature)) { std::fprintf(stderr, "sigGen tcId %d: signature mismatch\n", v.tc); ++failures; }
    }

    // Marscoin P2WPQH context: round trip, and rejection under other contexts.
    const char* ctx = "marscoin-p2wpqh-v1";
    const uint8_t seed[48] = {0x42};
    const uint8_t addrnd[16] = {0x17};
    const uint8_t msg[32] = {0x5a};
    std::vector<uint8_t> pk(slh_pk_sz(prm)), sk(slh_sk_sz(prm)), s(slh_sig_sz(prm));
    slh_keygen_internal(sk.data(), pk.data(), seed, seed + 16, seed + 32, prm);
    const size_t len = slh_sign(s.data(), msg, sizeof(msg), reinterpret_cast<const uint8_t*>(ctx), std::strlen(ctx), sk.data(), addrnd, prm);
    if (len != s.size() ||
        slh_verify(msg, sizeof(msg), s.data(), len, reinterpret_cast<const uint8_t*>(ctx), std::strlen(ctx), pk.data(), prm) != 1 ||
        slh_verify(msg, sizeof(msg), s.data(), len, nullptr, 0, pk.data(), prm) == 1) {
        std::fprintf(stderr, "Marscoin context round trip failed\n"); ++failures;
    }

    std::printf("keyGen=%zu sigVer=%zu sigGen=%zu failures=%d\n", sizeof(KEYGEN) / sizeof(KEYGEN[0]), sizeof(SIGVER) / sizeof(SIGVER[0]), sizeof(SIGGEN) / sizeof(SIGGEN[0]), failures);
    return failures == 0 ? 0 : 1;
}
CPP

for src in slh_dsa slh_sha2 sha2_256 sha2_512; do
  "$CC" -std=c99 -O2 -c "$SLHDSA_DIR/$src.c" -o "$WORK_DIR/$src.o"
done
"$CXX" -std=c++17 -O1 -I"$WORK_DIR" -I"$SLHDSA_DIR" "$WORK_DIR/kat.cpp" "$WORK_DIR"/slh_dsa.o "$WORK_DIR"/slh_sha2.o "$WORK_DIR"/sha2_256.o "$WORK_DIR"/sha2_512.o -o "$WORK_DIR/kat"
"$WORK_DIR/kat"
echo "SLH-DSA KAT passed."
