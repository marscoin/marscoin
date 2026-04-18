// Copyright (c) 2026 The Marscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_BLOCKWEIGHT_H
#define BITCOIN_CONSENSUS_BLOCKWEIGHT_H

#include <cstdint>

class CBlockIndex;

namespace Consensus {
struct Params;
} // namespace Consensus

/**
 * Adaptive Block Weight Limit (ABWL).
 *
 * Based on BCH CHIP-2023-04 ABLA (Adaptive Blocksize Limit Algorithm),
 * adapted for Marscoin's weight-based accounting system.
 *
 * The algorithm uses two interacting functions:
 * - Control function (epsilon): EWMA tracking actual block weight usage.
 *   Grows slowly when blocks are above the neutral point, shrinks when below.
 * - Elastic buffer (beta): Surge capacity that accumulates during quiet
 *   periods and is consumed during demand spikes.
 *
 * The block weight limit = epsilon + beta, clamped to [floor, temporary_max].
 */

struct ABWLState {
    int64_t epsilon{0}; //!< control function value
    int64_t beta{0};    //!< elastic buffer value
};

/**
 * Compute the ABWL state for the next block given the previous block's state
 * and actual weight.
 *
 * @param prev_epsilon  Control function value of the previous block.
 * @param prev_beta     Elastic buffer value of the previous block.
 * @param prev_weight   Actual weight of the previous block (clamped to prev limit).
 * @return              Updated ABWL state for the next block.
 */
ABWLState ComputeNextABWLState(int64_t prev_epsilon, int64_t prev_beta, int64_t prev_weight);

/**
 * Get the adaptive block weight limit for a block building on pindexPrev.
 *
 * Before the activation height, returns the legacy fixed MAX_BLOCK_WEIGHT.
 * At and after activation, computes the dynamic limit from the ABWL state.
 *
 * @param pindexPrev    Index of the previous block (nullptr for genesis).
 * @param params        Consensus parameters (contains activation height).
 * @return              Maximum allowed block weight for the next block.
 */
int64_t GetAdaptiveBlockWeightLimit(const CBlockIndex* pindexPrev, const Consensus::Params& params);

/**
 * Compute the initial ABWL state at the activation height.
 * Epsilon and beta are each set to ABWL_WEIGHT_FLOOR / 2 so that
 * the initial limit (epsilon + beta) equals the legacy fixed limit exactly.
 */
ABWLState GetInitialABWLState();

#endif // BITCOIN_CONSENSUS_BLOCKWEIGHT_H
