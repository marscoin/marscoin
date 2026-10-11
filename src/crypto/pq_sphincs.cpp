// Copyright (c) 2026 The Marscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/pq_sphincs.h>

#include <crypto/slhdsa/slh_dsa.h>

namespace pq::sphincs {

namespace {

const slh_param_t* Params(const ParameterSet parameter_set)
{
    switch (parameter_set) {
    case ParameterSet::SLH_DSA_SHA2_128S:
        return &slh_dsa_sha2_128s;
    }
    return nullptr;
}

const uint8_t* SigningContextData()
{
    return reinterpret_cast<const uint8_t*>(P2WPQH_SIGNING_CONTEXT.data());
}

} // namespace

bool GenerateKeypairFromSeeds(const ParameterSet parameter_set, const Span<const unsigned char> sk_seed, const Span<const unsigned char> sk_prf,
                              const Span<const unsigned char> pk_seed, std::vector<unsigned char>& public_key,
                              std::vector<unsigned char>& private_key, std::string& error)
{
    public_key.clear();
    private_key.clear();

    const slh_param_t* params = Params(parameter_set);
    if (params == nullptr) {
        error = "Unsupported SPHINCS+ parameter set";
        return false;
    }
    if (sk_seed.size() != SPHINCS_SEED_SIZE_SHA2_128S || sk_prf.size() != SPHINCS_SEED_SIZE_SHA2_128S ||
        pk_seed.size() != SPHINCS_SEED_SIZE_SHA2_128S) {
        error = "Invalid SLH-DSA seed length";
        return false;
    }

    public_key.resize(slh_pk_sz(params));
    private_key.resize(slh_sk_sz(params));
    slh_keygen_internal(private_key.data(), public_key.data(), sk_seed.data(), sk_prf.data(), pk_seed.data(), params);

    error.clear();
    return true;
}

bool SignMessageWithRandomness(const ParameterSet parameter_set, const Span<const unsigned char> private_key, const Span<const unsigned char> message,
                               const Span<const unsigned char> additional_randomness, std::vector<unsigned char>& payload_out,
                               std::string& error)
{
    payload_out.clear();

    if (private_key.empty()) {
        error = "Private key payload is empty";
        return false;
    }
    if (message.empty()) {
        error = "Message payload is empty";
        return false;
    }

    const slh_param_t* params = Params(parameter_set);
    if (params == nullptr) {
        error = "Unsupported SPHINCS+ parameter set";
        return false;
    }
    if (private_key.size() != slh_sk_sz(params)) {
        error = "Invalid SPHINCS+ private key length";
        return false;
    }
    if (!additional_randomness.empty() && additional_randomness.size() != SPHINCS_SEED_SIZE_SHA2_128S) {
        error = "Invalid SLH-DSA signing randomness length";
        return false;
    }

    std::vector<unsigned char> signature(slh_sig_sz(params));
    const size_t signature_len = slh_sign(signature.data(), message.data(), message.size(),
                                          SigningContextData(), P2WPQH_SIGNING_CONTEXT.size(), private_key.data(),
                                          additional_randomness.empty() ? nullptr : additional_randomness.data(), params);
    if (signature_len != signature.size()) {
        error = "Failed to compute SLH-DSA signature";
        return false;
    }

    payload_out.assign(1, static_cast<unsigned char>(parameter_set));
    payload_out.insert(payload_out.end(), signature.begin(), signature.end());

    const std::string format_error = ValidateSignatureEncoding(payload_out);
    if (!format_error.empty()) {
        payload_out.clear();
        error = format_error;
        return false;
    }

    error.clear();
    return true;
}

bool VerifyMessage(const ParameterSet parameter_set, const Span<const unsigned char> public_key, const Span<const unsigned char> message, const Span<const unsigned char> payload, std::string& error)
{
    if (public_key.empty()) {
        error = "Public key payload is empty";
        return false;
    }
    if (message.empty()) {
        error = "Message payload is empty";
        return false;
    }

    const std::string format_error = ValidateSignatureEncoding(payload);
    if (!format_error.empty()) {
        error = format_error;
        return false;
    }

    if (payload.front() != static_cast<unsigned char>(parameter_set)) {
        error = "Signature parameter set does not match verifier expectation";
        return false;
    }

    const slh_param_t* params = Params(parameter_set);
    if (params == nullptr) {
        error = "Unsupported SPHINCS+ parameter set";
        return false;
    }
    if (public_key.size() != slh_pk_sz(params)) {
        error = "Invalid SPHINCS+ public key length";
        return false;
    }

    const Span<const unsigned char> signature = payload.subspan(1);
    if (slh_verify(message.data(), message.size(), signature.data(), signature.size(),
                   SigningContextData(), P2WPQH_SIGNING_CONTEXT.size(), public_key.data(), params) != 1) {
        error = "SPHINCS+ signature verification failed";
        return false;
    }

    error.clear();
    return true;
}

} // namespace pq::sphincs
