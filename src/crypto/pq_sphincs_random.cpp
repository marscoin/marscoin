// Copyright (c) 2026 The Marscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/pq_sphincs.h>

#include <random.h>
#include <support/cleanse.h>

#include <array>

namespace pq::sphincs {

bool GenerateKeypair(const ParameterSet parameter_set, std::vector<unsigned char>& public_key, std::vector<unsigned char>& private_key, std::string& error)
{
    if (!IsSupportedParameterSet(static_cast<uint8_t>(parameter_set))) {
        public_key.clear();
        private_key.clear();
        error = "Unsupported SPHINCS+ parameter set";
        return false;
    }

    // FIPS 205 slh_keygen (Algorithm 21) draws SK.seed, SK.prf and PK.seed.
    std::array<unsigned char, SPHINCS_SEED_SIZE_SHA2_128S> sk_seed, sk_prf, pk_seed;
    GetStrongRandBytes(sk_seed);
    GetStrongRandBytes(sk_prf);
    GetStrongRandBytes(pk_seed);
    const bool ok = GenerateKeypairFromSeeds(parameter_set, sk_seed, sk_prf, pk_seed, public_key, private_key, error);
    memory_cleanse(sk_seed.data(), sk_seed.size());
    memory_cleanse(sk_prf.data(), sk_prf.size());
    return ok;
}

bool SignMessage(const ParameterSet parameter_set, const Span<const unsigned char> private_key, const Span<const unsigned char> message, std::vector<unsigned char>& payload_out, std::string& error)
{
    // Hedged signing: fresh opt_rand protects against fault and side-channel
    // attacks on the deterministic variant.
    std::array<unsigned char, SPHINCS_SEED_SIZE_SHA2_128S> additional_randomness;
    GetStrongRandBytes(additional_randomness);
    const bool ok = SignMessageWithRandomness(parameter_set, private_key, message, additional_randomness, payload_out, error);
    memory_cleanse(additional_randomness.data(), additional_randomness.size());
    return ok;
}

} // namespace pq::sphincs
