// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_CONSENSUS_H
#define BITCOIN_CONSENSUS_CONSENSUS_H

#include <cstdlib>
#include <stdint.h>

/** The maximum allowed size for a serialized block, in bytes (only for buffer size limits) */
static const unsigned int MAX_BLOCK_SERIALIZED_SIZE = 4000000;
/** The maximum allowed weight for a block, see BIP 141 (network rule) */
static const unsigned int MAX_BLOCK_WEIGHT = 4000000;
/** The maximum allowed number of signature check operations in a block (network rule) */
static const int64_t MAX_BLOCK_SIGOPS_COST = 80000;
/** Coinbase transaction outputs can only be spent after this number of new blocks (network rule) */
static const int COINBASE_MATURITY = 100;

static const int WITNESS_SCALE_FACTOR = 4;

static const size_t MIN_TRANSACTION_WEIGHT = WITNESS_SCALE_FACTOR * 60; // 60 is the lower bound for the size of a valid serialized CTransaction
static const size_t MIN_SERIALIZABLE_TRANSACTION_WEIGHT = WITNESS_SCALE_FACTOR * 10; // 10 is the lower bound for the size of a serialized CTransaction

/** Adaptive Block Weight Limit (ABWL) parameters.
 *  Based on BCH CHIP-2023-04 ABLA, adapted for weight-based accounting.
 *  The algorithm uses an EWMA control function and elastic buffer to
 *  dynamically adjust the block weight limit based on actual usage. */
static constexpr int64_t ABWL_WEIGHT_FLOOR = 4000000;
static constexpr int64_t ABWL_TEMPORARY_MAX = 128000000;
/** Zeta (asymmetry factor) = 3/2, stored as integer ratio. */
static constexpr int64_t ABWL_ZETA_NUM = 3;
static constexpr int64_t ABWL_ZETA_DEN = 2;
/** Gamma (per-block forget factor) = 1/ABWL_GAMMA_DIVISOR. */
static constexpr int64_t ABWL_GAMMA_DIVISOR = 37938;
/** Delta (elastic buffer gearing ratio). */
static constexpr int64_t ABWL_DELTA = 10;
/** Theta (buffer decay rate) = 1/ABWL_THETA_DIVISOR. */
static constexpr int64_t ABWL_THETA_DIVISOR = 37938;

/** Flags for nSequence and nLockTime locks */
/** Interpret sequence numbers as relative lock-time constraints. */
static constexpr unsigned int LOCKTIME_VERIFY_SEQUENCE = (1 << 0);

/**
 * Maximum number of seconds that the timestamp of the first
 * block of a difficulty adjustment period is allowed to
 * be earlier than the last block of the previous period (BIP94).
 */
static constexpr int64_t MAX_TIMEWARP = 600;

#endif // BITCOIN_CONSENSUS_CONSENSUS_H
