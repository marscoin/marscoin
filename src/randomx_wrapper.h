// Copyright (c) 2026 The Marscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RANDOMX_WRAPPER_H
#define BITCOIN_RANDOMX_WRAPPER_H

#include "span.h"

#include <array>
#include <string>
#include <vector>

namespace randomx {

struct CacheHandle {
    std::vector<unsigned char> key;
    void* cache;
    void* vm;
    bool initialized;

    CacheHandle() : cache(nullptr), vm(nullptr), initialized(false) {}
};

bool InitCache(CacheHandle& cache, Span<const unsigned char> key, std::string& error);
bool HashOnce(const CacheHandle& cache, Span<const unsigned char> input, std::array<unsigned char, 32>& hash_out, std::string& error);
void ReleaseCache(CacheHandle& cache);

} // namespace randomx

#endif // BITCOIN_RANDOMX_WRAPPER_H
