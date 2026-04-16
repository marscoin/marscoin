// Copyright (c) 2026 The Marscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CRYPTO_PQ_SPHINCS_H
#define BITCOIN_CRYPTO_PQ_SPHINCS_H

#include <span.h>

#include <cstdint>
#include <string>
#include <vector>

namespace pq::sphincs {

enum class ParameterSet : uint8_t {
    SLH_DSA_SHA2_128S = 0x00,
};

static constexpr size_t SPHINCS_SIGNATURE_SIZE_SHA2_128S = 7856;
static constexpr size_t SPHINCS_PUBLIC_KEY_SIZE_SHA2_128S = 32;
static constexpr size_t SPHINCS_SECRET_KEY_SIZE_SHA2_128S = 64;

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
 * Validate scaffold SPHINCS+ payload encoding.
 *
 * This checks only length/format and does not perform cryptographic verify.
 * Expected scaffold payload format:
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

bool IsOQSBackendAvailable(ParameterSet parameter_set, std::string& error);
bool GenerateKeypair(ParameterSet parameter_set, std::vector<unsigned char>& public_key, std::vector<unsigned char>& private_key, std::string& error);
bool SignMessage(ParameterSet parameter_set, Span<const unsigned char> private_key, Span<const unsigned char> message, std::vector<unsigned char>& payload_out, std::string& error);
bool VerifyMessage(ParameterSet parameter_set, Span<const unsigned char> public_key, Span<const unsigned char> message, Span<const unsigned char> payload, std::string& error);

} // namespace pq::sphincs

#endif // BITCOIN_CRYPTO_PQ_SPHINCS_H
