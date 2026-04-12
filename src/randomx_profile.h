// Copyright (c) 2026 The Marscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RANDOMX_PROFILE_H
#define BITCOIN_RANDOMX_PROFILE_H

#include <cstdint>
#include <string>

namespace randomx {

enum Flags : uint32_t {
    FLAG_DEFAULT = 0,
    FLAG_LARGE_PAGES = 1 << 0,
    FLAG_HARD_AES = 1 << 1,
    FLAG_FULL_MEM = 1 << 2,
    FLAG_JIT = 1 << 3,
    FLAG_SECURE = 1 << 4,
    FLAG_ARGON2_SSSE3 = 1 << 5,
    FLAG_ARGON2_AVX2 = 1 << 6,
    FLAG_V2 = 1 << 7,
};

struct ConsensusProfile {
    std::string upstream_repo;
    std::string upstream_tag;
    std::string upstream_commit;
    uint32_t required_flags;
};

inline ConsensusProfile GetConsensusProfileV1()
{
    ConsensusProfile profile;
    profile.upstream_repo = "tevador/RandomX";
    profile.upstream_tag = "v2.0";
    profile.upstream_commit = "e0db3c4a8de36d77f50c12f7099bc37401cab88c";
    profile.required_flags = FLAG_V2;
    return profile;
}

inline bool IsConsensusProfilePinned(const ConsensusProfile& profile)
{
    return profile.upstream_tag == "v2.0" &&
           profile.upstream_commit.size() == 40 &&
           (profile.required_flags & FLAG_V2) != 0;
}

inline std::string ValidateScaffoldFlags(const uint32_t flags)
{
    if ((flags & FLAG_V2) == 0) {
        return "RandomX v2 flag is required by consensus profile";
    }
    return std::string();
}

} // namespace randomx

#endif // BITCOIN_RANDOMX_PROFILE_H
