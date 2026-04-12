// Copyright (c) 2026 The Marscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "randomx_wrapper.h"

#include "crypto/sha256.h"
#include "crypto/randomx_vendor/src/randomx.h"

namespace {

static const unsigned char RANDOMX_SCAFFOLD_DOMAIN[] = {
    'm', 'a', 'r', 's', 'c', 'o', 'i', 'n', '-', 'r', 'a', 'n', 'd', 'o', 'm', 'x', '-', 's', 'c', 'a', 'f', 'f', 'o', 'l', 'd', '-', 'v', '1'
};

} // namespace

namespace randomx {

bool InitCache(CacheHandle& cache, Span<const unsigned char> key, std::string& error)
{
    if (key.empty()) {
        error = "RandomX scaffold cache key cannot be empty";
        return false;
    }

    cache.key.assign(key.begin(), key.end());
    cache.initialized = true;
    error.clear();
    return true;
}

bool HashOnce(const CacheHandle& cache, Span<const unsigned char> input, std::array<unsigned char, 32>& hash_out, std::string& error)
{
    if (!cache.initialized) {
        error = "RandomX scaffold cache is not initialized";
        return false;
    }

    if (input.empty()) {
        error = "RandomX scaffold input cannot be empty";
        return false;
    }

    // Non-activating scaffold hash path. This intentionally does not perform
    // RandomX PoW execution yet, but it uses the vendored header constants to
    // lock in consensus-facing flag expectations while providing deterministic
    // vectors for CI.
    static_assert(RANDOMX_FLAG_V2 == 128, "Unexpected RandomX v2 flag value");
    CSHA256 hasher;
    hasher.Write(RANDOMX_SCAFFOLD_DOMAIN, sizeof(RANDOMX_SCAFFOLD_DOMAIN));
    hasher.Write(cache.key.data(), cache.key.size());
    hasher.Write(input.data(), input.size());
    hasher.Finalize(hash_out.data());

    error.clear();
    return true;
}

} // namespace randomx
