// Copyright (c) 2026 The Marscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "randomx_profile.h"
#include "randomx_scaffold.h"

#include "crypto/randomx_vendor/src/randomx.h"

#include <sstream>

static_assert(RANDOMX_FLAG_V2 == 128, "Unexpected RandomX v2 flag value");

namespace randomx {

std::string VendorScaffoldSummary()
{
    const ConsensusProfile profile = GetConsensusProfileV1();

    std::ostringstream stream;
    stream << "RandomX vendor scaffold pinned to "
           << profile.upstream_repo
           << "@"
           << profile.upstream_tag
           << " ("
           << profile.upstream_commit
           << ")"
           << ", required_flags="
           << profile.required_flags;
    return stream.str();
}

} // namespace randomx
