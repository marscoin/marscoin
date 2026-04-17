#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OQS_INSTALL="${OQS_INSTALL:-$ROOT_DIR/src/crypto/oqs_vendor/build/install}"
OPENSSL_PREFIX="${OPENSSL_PREFIX:-}"

if [[ -z "$OPENSSL_PREFIX" ]]; then
  if command -v brew >/dev/null 2>&1; then
    OPENSSL_PREFIX="$(brew --prefix openssl@3 2>/dev/null || true)"
  fi
fi

EXPECTED_PUB="8a69a3c0db2ef0d7c439fff27bab906d2b8bcb8c7048556e30bc3bb8ffc8403b"
EXPECTED_PAYLOAD_HASH="6b2f4f0a998f29de8919c170a8dad69a44c735e37deb3168c2a0b95d87c2fd23"

if [[ ! -f "$OQS_INSTALL/include/oqs/oqs.h" ]] || [[ ! -f "$OQS_INSTALL/lib/liboqs.a" ]]; then
  echo "Missing vendored liboqs install artifacts under: $OQS_INSTALL" >&2
  echo "Run src/crypto/oqs_vendor/build-liboqs-vendor.sh first." >&2
  exit 1
fi

cat > /tmp/marscoin_oqs_kat.cpp <<'CPP'
#include <oqs/oqs.h>
#include <oqs/rand.h>
#include <openssl/sha.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

static uint64_t g_state = 0;

static void deterministic_rng(uint8_t *out, size_t outlen) {
    uint64_t x = g_state;
    for (size_t i = 0; i < outlen; ++i) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        out[i] = static_cast<uint8_t>(x & 0xFF);
    }
    g_state = x;
}

static void reset_rng(uint64_t seed) {
    g_state = seed;
    OQS_randombytes_custom_algorithm(deterministic_rng);
}

static void print_hex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) std::printf("%02x", data[i]);
    std::printf("\n");
}

int main() {
    OQS_SIG* sig = OQS_SIG_new(OQS_SIG_alg_sphincs_sha2_128s_simple);
    if (!sig) {
        std::fprintf(stderr, "SPHINCS SHA2-128s backend unavailable\n");
        return 2;
    }

    reset_rng(0x4d415253514e4554ULL);
    std::vector<uint8_t> pub(sig->length_public_key), priv(sig->length_secret_key);
    if (OQS_SIG_keypair(sig, pub.data(), priv.data()) != OQS_SUCCESS) {
        std::fprintf(stderr, "keypair generation failed\n");
        OQS_SIG_free(sig);
        return 3;
    }

    const char msg[] = "marsqnet-sign-test";
    reset_rng(0x5349474e41545552ULL);
    std::vector<uint8_t> signature(sig->length_signature);
    size_t sig_len = 0;
    if (OQS_SIG_sign(sig, signature.data(), &sig_len, reinterpret_cast<const uint8_t*>(msg), std::strlen(msg), priv.data()) != OQS_SUCCESS) {
        std::fprintf(stderr, "signature generation failed\n");
        OQS_SIG_free(sig);
        return 4;
    }
    signature.resize(sig_len);

    std::vector<uint8_t> payload;
    payload.push_back(0x00);
    payload.insert(payload.end(), signature.begin(), signature.end());

    uint8_t digest[SHA256_DIGEST_LENGTH];
    SHA256(payload.data(), payload.size(), digest);

    std::printf("PUB=");
    print_hex(pub.data(), pub.size());
    std::printf("PAYLOAD_SHA256=");
    print_hex(digest, sizeof(digest));

    const int verify_rc = OQS_SIG_verify(sig, reinterpret_cast<const uint8_t*>(msg), std::strlen(msg), signature.data(), signature.size(), pub.data());
    OQS_SIG_free(sig);
    if (verify_rc != OQS_SUCCESS) {
        std::fprintf(stderr, "verify failed\n");
        return 5;
    }
    return 0;
}
CPP

OPENSSL_INCLUDE_FLAGS=""
OPENSSL_LIB_FLAGS=""
if [[ -n "$OPENSSL_PREFIX" ]]; then
  OPENSSL_INCLUDE_FLAGS="-I$OPENSSL_PREFIX/include"
  OPENSSL_LIB_FLAGS="-L$OPENSSL_PREFIX/lib"
fi

g++ -std=c++17 \
  $OPENSSL_INCLUDE_FLAGS \
  -I"$OQS_INSTALL/include" \
  /tmp/marscoin_oqs_kat.cpp \
  "$OQS_INSTALL/lib/liboqs.a" \
  $OPENSSL_LIB_FLAGS \
  -lcrypto \
  -o /tmp/marscoin_oqs_kat

OUTPUT="$(/tmp/marscoin_oqs_kat)"
echo "$OUTPUT"

PUB="$(printf '%s\n' "$OUTPUT" | sed -n 's/^PUB=//p')"
PAYLOAD_HASH="$(printf '%s\n' "$OUTPUT" | sed -n 's/^PAYLOAD_SHA256=//p')"

if [[ "$PUB" != "$EXPECTED_PUB" ]]; then
  echo "KAT failure: unexpected public key" >&2
  exit 10
fi
if [[ "$PAYLOAD_HASH" != "$EXPECTED_PAYLOAD_HASH" ]]; then
  echo "KAT failure: unexpected payload hash" >&2
  exit 11
fi

echo "OQS SPHINCS KAT passed."
