// Copyright (c) 2026 The Marscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "randomx_wrapper.h"

#include "crypto/randomx_vendor/src/randomx.h"

namespace randomx {

namespace {

randomx_flags BuildFlags(const bool allow_jit)
{
    randomx_flags flags = randomx_get_flags();
    flags = static_cast<randomx_flags>(flags | RANDOMX_FLAG_V2);
    flags = static_cast<randomx_flags>(flags & ~RANDOMX_FLAG_FULL_MEM);
    if (!allow_jit) {
        flags = static_cast<randomx_flags>(flags & ~RANDOMX_FLAG_JIT);
    }
    return flags;
}

} // namespace

bool InitCache(CacheHandle& cache, Span<const unsigned char> key, std::string& error)
{
    if (key.empty()) {
        error = "RandomX cache key cannot be empty";
        return false;
    }

    ReleaseCache(cache);

    cache.key.assign(key.begin(), key.end());

    randomx_flags flags = BuildFlags(true);
    randomx_cache* vm_cache = randomx_alloc_cache(flags);
    if (vm_cache == nullptr) {
        flags = BuildFlags(false);
        vm_cache = randomx_alloc_cache(flags);
    }
    if (vm_cache == nullptr) {
        error = "Unable to allocate RandomX cache";
        return false;
    }

    randomx_init_cache(vm_cache, cache.key.data(), cache.key.size());

    randomx_vm* vm = randomx_create_vm(flags, vm_cache, nullptr);
    if (vm == nullptr) {
        randomx_release_cache(vm_cache);
        error = "Unable to create RandomX VM";
        return false;
    }

    cache.cache = vm_cache;
    cache.vm = vm;
    cache.initialized = true;
    error.clear();
    return true;
}

bool HashOnce(const CacheHandle& cache, Span<const unsigned char> input, std::array<unsigned char, 32>& hash_out, std::string& error)
{
    if (!cache.initialized) {
        error = "RandomX cache is not initialized";
        return false;
    }

    if (input.empty()) {
        error = "RandomX input cannot be empty";
        return false;
    }

    static_assert(RANDOMX_FLAG_V2 == 128, "Unexpected RandomX v2 flag value");
    randomx_vm* vm = reinterpret_cast<randomx_vm*>(cache.vm);
    if (vm == nullptr) {
        error = "RandomX VM handle is missing";
        return false;
    }

    randomx_calculate_hash(vm, input.data(), input.size(), hash_out.data());

    error.clear();
    return true;
}

void ReleaseCache(CacheHandle& cache)
{
    if (cache.vm != nullptr) {
        randomx_destroy_vm(reinterpret_cast<randomx_vm*>(cache.vm));
        cache.vm = nullptr;
    }
    if (cache.cache != nullptr) {
        randomx_release_cache(reinterpret_cast<randomx_cache*>(cache.cache));
        cache.cache = nullptr;
    }
    cache.key.clear();
    cache.initialized = false;
}

} // namespace randomx
