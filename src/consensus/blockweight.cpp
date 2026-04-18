// Copyright (c) 2026 The Marscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/blockweight.h>

#include <chain.h>
#include <consensus/consensus.h>
#include <consensus/params.h>

#include <algorithm>

ABWLState GetInitialABWLState()
{
    return ABWLState{ABWL_WEIGHT_FLOOR / 2, ABWL_WEIGHT_FLOOR / 2};
}

ABWLState ComputeNextABWLState(int64_t prev_epsilon, int64_t prev_beta, int64_t prev_weight)
{
    const int64_t prev_limit = prev_epsilon + prev_beta;
    const int64_t epsilon_floor = ABWL_WEIGHT_FLOOR / 2;
    const int64_t beta_floor = ABWL_WEIGHT_FLOOR / 2;

    // Clamp input: actual weight cannot exceed the limit
    const int64_t x = std::min(prev_weight, prev_limit);

    // Compute zeta-scaled input: zeta * x = (ZETA_NUM * x) / ZETA_DEN
    // Use 128-bit intermediate to avoid overflow on large values.
    const __int128 zeta_x = static_cast<__int128>(ABWL_ZETA_NUM) * x / ABWL_ZETA_DEN;
    const __int128 zeta_beta = static_cast<__int128>(ABWL_ZETA_NUM) * prev_beta / ABWL_ZETA_DEN;
    const __int128 zeta_limit = static_cast<__int128>(ABWL_ZETA_NUM) * prev_limit / ABWL_ZETA_DEN;

    int64_t epsilon;
    int64_t beta;

    if (static_cast<int64_t>(zeta_x) > prev_epsilon) {
        // Block was above neutral point -> grow control function
        // Discount growth by elastic buffer contribution to avoid double-counting
        const __int128 raw_growth = zeta_x - prev_epsilon;
        const __int128 denom = zeta_limit - prev_epsilon;
        __int128 discount = 0;
        if (denom > 0) {
            discount = zeta_beta * raw_growth / denom;
        }
        const __int128 adjusted = raw_growth - discount;
        epsilon = prev_epsilon + static_cast<int64_t>(adjusted / ABWL_GAMMA_DIVISOR);

        // Buffer: decay + gearing from epsilon growth
        const int64_t epsilon_growth = epsilon - prev_epsilon;
        const int64_t decay = prev_beta / ABWL_THETA_DIVISOR;
        beta = prev_beta - decay + ABWL_DELTA * epsilon_growth;
        beta = std::max(beta, beta_floor);
    } else {
        // Block was at or below neutral point -> shrink control function
        const __int128 shrink = zeta_x - prev_epsilon; // negative
        epsilon = prev_epsilon + static_cast<int64_t>(shrink / ABWL_GAMMA_DIVISOR);
        epsilon = std::max(epsilon, epsilon_floor);

        // Buffer: decay only
        const int64_t decay = prev_beta / ABWL_THETA_DIVISOR;
        beta = prev_beta - decay;
        beta = std::max(beta, beta_floor);
    }

    // Clamp epsilon and beta to prevent runaway growth
    epsilon = std::min(epsilon, ABWL_TEMPORARY_MAX);
    beta = std::min(beta, ABWL_TEMPORARY_MAX);

    return ABWLState{epsilon, beta};
}

int64_t GetAdaptiveBlockWeightLimit(const CBlockIndex* pindexPrev, const Consensus::Params& params)
{
    // Before activation or if not configured, use legacy fixed limit
    if (params.nABWLActivationHeight == 0 || pindexPrev == nullptr) {
        return MAX_BLOCK_WEIGHT;
    }

    const int next_height = pindexPrev->nHeight + 1;
    if (next_height < params.nABWLActivationHeight) {
        return MAX_BLOCK_WEIGHT;
    }

    // At exactly the activation height, the previous block has no ABWL state.
    // Use the initial state: epsilon + beta = ABWL_WEIGHT_FLOOR.
    if (next_height == params.nABWLActivationHeight) {
        return ABWL_WEIGHT_FLOOR;
    }

    // Compute from previous block's persisted ABWL state
    const int64_t limit = pindexPrev->nABWL_epsilon + pindexPrev->nABWL_beta;
    return std::clamp(limit, ABWL_WEIGHT_FLOOR, ABWL_TEMPORARY_MAX);
}
