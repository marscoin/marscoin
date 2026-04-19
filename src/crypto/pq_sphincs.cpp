// Copyright (c) 2026 The Marscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <config/bitcoin-config.h> // IWYU pragma: keep

#include <crypto/pq_sphincs.h>

#ifdef ENABLE_PQ_OQS_VENDOR
#include <oqs/oqs.h>
#endif

namespace pq::sphincs {

namespace {

#ifdef ENABLE_PQ_OQS_VENDOR
const char* OQSAlgName(const ParameterSet parameter_set)
{
    switch (parameter_set) {
    case ParameterSet::SLH_DSA_SHA2_128S:
        return OQS_SIG_alg_sphincs_sha2_128s_simple;
    }
    return nullptr;
}
#endif

} // namespace

bool IsOQSBackendAvailable(const ParameterSet parameter_set, std::string& error)
{
#ifndef ENABLE_PQ_OQS_VENDOR
    (void)parameter_set;
    error = "OQS backend not enabled in this build (configure with --enable-pq-oqs-vendor)";
    return false;
#else
    const char* alg_name = OQSAlgName(parameter_set);
    if (alg_name == nullptr) {
        error = "Unsupported SPHINCS+ parameter set";
        return false;
    }
    OQS_SIG* sig = OQS_SIG_new(alg_name);
    if (sig == nullptr) {
        error = "OQS backend does not provide requested algorithm";
        return false;
    }
    OQS_SIG_free(sig);
    error.clear();
    return true;
#endif
}

bool GenerateKeypair(const ParameterSet parameter_set, std::vector<unsigned char>& public_key, std::vector<unsigned char>& private_key, std::string& error)
{
#ifndef ENABLE_PQ_OQS_VENDOR
    (void)parameter_set;
    public_key.clear();
    private_key.clear();
    error = "OQS backend not enabled in this build (configure with --enable-pq-oqs-vendor)";
    return false;
#else
    const char* alg_name = OQSAlgName(parameter_set);
    if (alg_name == nullptr) {
        error = "Unsupported SPHINCS+ parameter set";
        return false;
    }
    OQS_SIG* sig = OQS_SIG_new(alg_name);
    if (sig == nullptr) {
        error = "Failed to initialize OQS signature context";
        return false;
    }

    public_key.resize(sig->length_public_key);
    private_key.resize(sig->length_secret_key);
    if (OQS_SIG_keypair(sig, public_key.data(), private_key.data()) != OQS_SUCCESS) {
        OQS_SIG_free(sig);
        public_key.clear();
        private_key.clear();
        error = "Failed to generate SPHINCS+ keypair via OQS backend";
        return false;
    }

    OQS_SIG_free(sig);
    error.clear();
    return true;
#endif
}

bool SignMessage(const ParameterSet parameter_set, const Span<const unsigned char> private_key, const Span<const unsigned char> message, std::vector<unsigned char>& payload_out, std::string& error)
{
    if (private_key.empty()) {
        error = "Private key payload is empty";
        return false;
    }
    if (message.empty()) {
        error = "Message payload is empty";
        return false;
    }

#ifndef ENABLE_PQ_OQS_VENDOR
    (void)parameter_set;
    payload_out.clear();
    error = "OQS backend not enabled in this build (configure with --enable-pq-oqs-vendor)";
    return false;
#else
    const char* alg_name = OQSAlgName(parameter_set);
    if (alg_name == nullptr) {
        error = "Unsupported SPHINCS+ parameter set";
        return false;
    }
    OQS_SIG* sig = OQS_SIG_new(alg_name);
    if (sig == nullptr) {
        error = "Failed to initialize OQS signature context";
        return false;
    }

    if (private_key.size() != sig->length_secret_key) {
        OQS_SIG_free(sig);
        error = "Invalid SPHINCS+ private key length";
        return false;
    }

    std::vector<unsigned char> signature(sig->length_signature);
    size_t signature_len{0};
    if (OQS_SIG_sign(sig, signature.data(), &signature_len, message.data(), message.size(), private_key.data()) != OQS_SUCCESS) {
        OQS_SIG_free(sig);
        error = "Failed to compute SPHINCS+ signature via OQS backend";
        return false;
    }

    signature.resize(signature_len);
    payload_out.assign(1, static_cast<unsigned char>(parameter_set));
    payload_out.insert(payload_out.end(), signature.begin(), signature.end());

    const std::string format_error = ValidateSignatureEncoding(payload_out);
    if (!format_error.empty()) {
        OQS_SIG_free(sig);
        payload_out.clear();
        error = format_error;
        return false;
    }

    OQS_SIG_free(sig);
    error.clear();
    return true;
#endif
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

#ifndef ENABLE_PQ_OQS_VENDOR
    (void)parameter_set;
    error = "OQS backend not enabled in this build (configure with --enable-pq-oqs-vendor)";
    return false;
#else
    const char* alg_name = OQSAlgName(parameter_set);
    if (alg_name == nullptr) {
        error = "Unsupported SPHINCS+ parameter set";
        return false;
    }
    OQS_SIG* sig = OQS_SIG_new(alg_name);
    if (sig == nullptr) {
        error = "Failed to initialize OQS signature context";
        return false;
    }

    if (public_key.size() != sig->length_public_key) {
        OQS_SIG_free(sig);
        error = "Invalid SPHINCS+ public key length";
        return false;
    }

    const unsigned char* signature = payload.data() + 1;
    const size_t signature_len = payload.size() - 1;
    if (OQS_SIG_verify(sig, message.data(), message.size(), signature, signature_len, public_key.data()) != OQS_SUCCESS) {
        OQS_SIG_free(sig);
        error = "SPHINCS+ signature verification failed";
        return false;
    }

    OQS_SIG_free(sig);
    error.clear();
    return true;
#endif
}

} // namespace pq::sphincs
