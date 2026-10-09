// Copyright (c) 2026 The Marscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CRYPTO_PQ_SPHINCS_H
#define BITCOIN_CRYPTO_PQ_SPHINCS_H

#include <span.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pq::sphincs {

/**
 * Signature parameter sets, identified by the byte committed into the P2WPQH
 * program (SHA256(id || pubkey)).
 *
 * 0x00 was round-3 SPHINCS+-SHA2-128s-simple and is retired: it is no longer
 * supported and is rejected like any unknown id.
 */
enum class ParameterSet : uint8_t {
    //! FIPS 205 SLH-DSA-SHA2-128s, pure interface, signed with P2WPQH_SIGNING_CONTEXT.
    SLH_DSA_SHA2_128S = 0x01,
};

static constexpr size_t SPHINCS_SIGNATURE_SIZE_SHA2_128S = 7856;
static constexpr size_t SPHINCS_PUBLIC_KEY_SIZE_SHA2_128S = 32;
static constexpr size_t SPHINCS_SECRET_KEY_SIZE_SHA2_128S = 64;
//! Security parameter n: the size of each key-generation seed and of the signing randomness.
static constexpr size_t SPHINCS_SEED_SIZE_SHA2_128S = 16;

/**
 * FIPS 205 context string for every P2WPQH signature. It domain-separates these
 * signatures from any other use of the same keys.
 */
static constexpr std::string_view P2WPQH_SIGNING_CONTEXT{"marscoin-p2wpqh-v1"};

inline constexpr bool IsSupportedParameterSet(const uint8_t value)
{
    return value == static_cast<uint8_t>(ParameterSet::SLH_DSA_SHA2_128S);
}

inline constexpr size_t SignatureSize(const ParameterSet parameter_set)
{
    switch (parameter_set) {
    case ParameterSet::SLH_DSA_SHA2_128S:
        return SPHINCS_SIGNATURE_SIZE_SHA2_128S;
    }
    return 0;
}

inline const char* ParameterSetName(const ParameterSet parameter_set)
{
    switch (parameter_set) {
    case ParameterSet::SLH_DSA_SHA2_128S:
        return "SLH-DSA-SHA2-128s";
    }
    return "unknown";
}

/**
 * Validate the signature payload encoding.
 *
 * This checks only length/format and does not perform cryptographic verify.
 * Payload format:
 *   [0]: parameter set id
 *   [1..n]: raw signature bytes for the selected parameter set
 */
inline std::string ValidateSignatureEncoding(const Span<const unsigned char> payload)
{
    if (payload.empty()) {
        return "Empty SPHINCS+ payload";
    }

    const uint8_t parameter_set{payload.front()};
    if (!IsSupportedParameterSet(parameter_set)) {
        return "Unsupported SPHINCS+ parameter set";
    }

    const auto expected_size = SignatureSize(static_cast<ParameterSet>(parameter_set));
    if (payload.size() != expected_size + 1) {
        return "Invalid SPHINCS+ payload length";
    }

    return {};
}

/*
 * SLH-DSA is provided by the vendored slhdsa-c implementation
 * (src/crypto/slhdsa/), which is part of every build.
 *
 * The functions below are deterministic and need no random number generator;
 * they are part of the consensus library.
 */

//! Verify a payload [id || signature] against P2WPQH_SIGNING_CONTEXT.
bool VerifyMessage(ParameterSet parameter_set, Span<const unsigned char> public_key, Span<const unsigned char> message, Span<const unsigned char> payload, std::string& error);

/**
 * FIPS 205 slh_keygen_internal (Algorithm 18): derive a key pair from the
 * three n-byte seeds SK.seed, SK.prf and PK.seed. The same seeds always give
 * the same key pair.
 */
bool GenerateKeypairFromSeeds(ParameterSet parameter_set, Span<const unsigned char> sk_seed, Span<const unsigned char> sk_prf,
                              Span<const unsigned char> pk_seed, std::vector<unsigned char>& public_key,
                              std::vector<unsigned char>& private_key, std::string& error);

/**
 * Sign with P2WPQH_SIGNING_CONTEXT and return the payload [id || signature].
 * additional_randomness is FIPS 205's opt_rand: n fresh random bytes for the
 * hedged variant, or empty for the deterministic variant. Prefer SignMessage,
 * which supplies fresh randomness.
 */
bool SignMessageWithRandomness(ParameterSet parameter_set, Span<const unsigned char> private_key, Span<const unsigned char> message,
                               Span<const unsigned char> additional_randomness, std::vector<unsigned char>& payload_out,
                               std::string& error);

/*
 * The functions below draw randomness from GetStrongRandBytes; they are part of
 * the common library (src/crypto/pq_sphincs_random.cpp).
 */

//! Generate a key pair from fresh random seeds.
bool GenerateKeypair(ParameterSet parameter_set, std::vector<unsigned char>& public_key, std::vector<unsigned char>& private_key, std::string& error);
//! Sign with P2WPQH_SIGNING_CONTEXT (hedged, with fresh randomness) and return the payload [id || signature].
bool SignMessage(ParameterSet parameter_set, Span<const unsigned char> private_key, Span<const unsigned char> message, std::vector<unsigned char>& payload_out, std::string& error);

} // namespace pq::sphincs

#endif // BITCOIN_CRYPTO_PQ_SPHINCS_H
