// Copyright (c) 2015-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <config/bitcoin-config.h> // IWYU pragma: keep

#include <chain.h>
#include <chainparams.h>
#include <pow.h>
#include <randomx_profile.h>
#ifdef ENABLE_RANDOMX_VENDOR
#include <randomx_wrapper.h>
#endif
#include <test/util/random.h>
#include <test/util/setup_common.h>
#include <util/chaintype.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(pow_tests, BasicTestingSetup)

namespace {

//! A chain of block indexes with fixed spacing and difficulty, for the
//! retarget tests. Chains that start at height 0 get skip pointers.
struct RetargetChain {
    std::vector<CBlockIndex> blocks;

    RetargetChain(int first_height, int count, int64_t first_time, int64_t spacing, uint32_t bits) : blocks(count)
    {
        for (int i = 0; i < count; ++i) {
            blocks[i].pprev = i ? &blocks[i - 1] : nullptr;
            blocks[i].nHeight = first_height + i;
            blocks[i].nTime = first_time + i * spacing;
            blocks[i].nBits = bits;
            if (first_height == 0) blocks[i].BuildSkip();
        }
    }

    CBlockIndex& Tip() { return blocks.back(); }
};

arith_uint256 Target(uint32_t bits)
{
    arith_uint256 target;
    target.SetCompact(bits);
    return target;
}

uint32_t Compact(const arith_uint256& target)
{
    return target.GetCompact();
}

//! A difficulty well inside every mainnet limit, about 2^224.
constexpr uint32_t TEST_BITS{0x1d00ffff};
constexpr int64_t START_TIME{1388590627};

} // namespace

/* Mainnet uses aserti3-2d (GravityAsert) from height 2999999 on, anchored at
 * that height. The test anchors it at height 100 of a 50,000-block chain:
 * the result depends only on the anchor and the last block. */
BOOST_AUTO_TEST_CASE(asert_retarget)
{
    Consensus::Params params{CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus()};
    params.fPowAlwaysAsert = true;
    params.nASERTAnchor = 100;
    const int64_t spacing{params.nASERTSpacing};
    const int64_t half_life{params.nASERTHalfLife};

    RetargetChain chain{0, 50'000, START_TIME, spacing, TEST_BITS};
    const CBlockIndex& anchor{chain.blocks[params.nASERTAnchor]};
    CBlockIndex& last{chain.Tip()};
    const int64_t height_diff{last.nHeight - anchor.nHeight};
    // The ideal time of the next block, as the formula counts it.
    const int64_t on_schedule{anchor.GetBlockTime() + spacing * (height_diff + 1)};

    last.nTime = on_schedule;
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&last, nullptr, params), TEST_BITS);

    // One half-life behind schedule doubles the target, one ahead halves it.
    last.nTime = on_schedule + half_life;
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&last, nullptr, params), Compact(Target(TEST_BITS) * 2));
    last.nTime = on_schedule - half_life;
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&last, nullptr, params), Compact(Target(TEST_BITS) / 2));

    // Between, the target moves monotonically.
    last.nTime = on_schedule + half_life / 2;
    const arith_uint256 mid{Target(GetNextWorkRequired(&last, nullptr, params))};
    BOOST_CHECK(mid > Target(TEST_BITS) && mid < Target(TEST_BITS) * 2);

    // Far behind schedule the target stops at the limit; far ahead it stays positive.
    last.nTime = on_schedule + 100 * half_life;
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&last, nullptr, params), UintToArith256(params.powLimit).GetCompact());
    last.nTime = on_schedule - 300 * half_life;
    BOOST_CHECK(Target(GetNextWorkRequired(&last, nullptr, params)) > 0);

    // Before the anchor, blocks are mined at the limit. Right after it, the
    // next block is due one spacing later, so the target is a little lower.
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&chain.blocks[99], nullptr, params), UintToArith256(params.powLimit).GetCompact());
    const arith_uint256 after_anchor{Target(GetNextWorkRequired(&chain.blocks[100], nullptr, params))};
    BOOST_CHECK(after_anchor < Target(TEST_BITS) && after_anchor > Target(TEST_BITS) / 2);
}

/* Mainnet heights 126000 to 2999998 use Dark Gravity Wave v3: the average
 * target of the last 24 blocks, scaled by their timespan against 24 target
 * spacings, within a factor of 3. */
BOOST_AUTO_TEST_CASE(dgw3_retarget)
{
    const Consensus::Params params{CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus()};
    constexpr int64_t spacing{123};
    constexpr int64_t target_timespan{24 * spacing};

    // On schedule. The 24 blocks span 23 intervals, so the result is 23/24 of
    // the average target, as on mainnet.
    RetargetChain steady{200'000, 30, START_TIME, spacing, TEST_BITS};
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&steady.Tip(), nullptr, params), Compact(Target(TEST_BITS) * (23 * spacing) / target_timespan));

    RetargetChain slower{200'000, 30, START_TIME, 2 * spacing, TEST_BITS};
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&slower.Tip(), nullptr, params), Compact(Target(TEST_BITS) * (23 * 2 * spacing) / target_timespan));

    // The timespan is clamped to a factor of 3 either way.
    RetargetChain very_slow{200'000, 30, START_TIME, 1000, TEST_BITS};
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&very_slow.Tip(), nullptr, params), Compact(Target(TEST_BITS) * 3));
    RetargetChain very_fast{200'000, 30, START_TIME, 1, TEST_BITS};
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&very_fast.Tip(), nullptr, params), Compact(Target(TEST_BITS) / 3));
}

/* Mainnet heights 70000 to 119999 use the legacy retarget: every 721 blocks
 * (one Mars-day of 88775 seconds at 123 seconds), within a factor of 4. */
BOOST_AUTO_TEST_CASE(legacy_retarget)
{
    const Consensus::Params params{CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus()};
    constexpr int64_t spacing{123};
    constexpr int64_t timespan{88775};
    constexpr int interval{timespan / spacing}; // 721
    constexpr int retarget_height{interval * 111}; // 80031, the height of the next block

    RetargetChain steady{retarget_height - 1 - interval, interval + 1, START_TIME, spacing, TEST_BITS};
    BOOST_REQUIRE_EQUAL(steady.Tip().nHeight + 1, retarget_height);
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&steady.Tip(), nullptr, params), Compact(Target(TEST_BITS) * (interval * spacing) / timespan));

    RetargetChain very_slow{retarget_height - 1 - interval, interval + 1, START_TIME, 2000, TEST_BITS};
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&very_slow.Tip(), nullptr, params), Compact(Target(TEST_BITS) * 4));
    // The lower clamp is timespan / 4 in integer division.
    RetargetChain very_fast{retarget_height - 1 - interval, interval + 1, START_TIME, 1, TEST_BITS};
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&very_fast.Tip(), nullptr, params), Compact(Target(TEST_BITS) * (timespan / 4) / timespan));

    // Between retargets the difficulty doesn't change.
    RetargetChain between{retarget_height, 10, START_TIME, 1, 0x1c7fffff};
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&between.Tip(), nullptr, params), 0x1c7fffffU);
}

/* Regtest doesn't retarget. */
BOOST_AUTO_TEST_CASE(no_retargeting)
{
    const Consensus::Params params{CreateChainParams(*m_node.args, ChainType::REGTEST)->GetConsensus()};
    BOOST_REQUIRE(params.fPowNoRetargeting);
    RetargetChain chain{0, 10, START_TIME, 1, 0x207fffff};
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&chain.Tip(), nullptr, params), 0x207fffffU);
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_negative_target)
{
    const auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    nBits = UintToArith256(consensus.powLimit).GetCompact(true);
    hash = uint256{1};
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_overflow_target)
{
    const auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits{~0x00800000U};
    hash = uint256{1};
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_too_easy_target)
{
    const auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    arith_uint256 nBits_arith = UintToArith256(consensus.powLimit);
    nBits_arith *= 2;
    nBits = nBits_arith.GetCompact();
    hash = uint256{1};
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_biger_hash_than_target)
{
    const auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    arith_uint256 hash_arith = UintToArith256(consensus.powLimit);
    nBits = hash_arith.GetCompact();
    hash_arith *= 2; // hash > nBits
    hash = ArithToUint256(hash_arith);
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_zero_target)
{
    const auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    arith_uint256 hash_arith{0};
    nBits = hash_arith.GetCompact();
    hash = ArithToUint256(hash_arith);
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(GetBlockProofEquivalentTime_test)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    std::vector<CBlockIndex> blocks(10000);
    for (int i = 0; i < 10000; i++) {
        blocks[i].pprev = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight = i;
        blocks[i].nTime = 1269211443 + i * chainParams->GetConsensus().nPowTargetSpacing;
        blocks[i].nBits = 0x207fffff; /* target 0x7fffff000... */
        blocks[i].nChainWork = i ? blocks[i - 1].nChainWork + GetBlockProof(blocks[i - 1]) : arith_uint256(0);
    }

    for (int j = 0; j < 1000; j++) {
        CBlockIndex *p1 = &blocks[InsecureRandRange(10000)];
        CBlockIndex *p2 = &blocks[InsecureRandRange(10000)];
        CBlockIndex *p3 = &blocks[InsecureRandRange(10000)];

        int64_t tdiff = GetBlockProofEquivalentTime(*p1, *p2, *p3, chainParams->GetConsensus());
        BOOST_CHECK_EQUAL(tdiff, p1->GetBlockTime() - p2->GetBlockTime());
    }
}

void sanity_check_chainparams(const ArgsManager& args, ChainType chain_type)
{
    const auto chainParams = CreateChainParams(args, chain_type);
    const auto consensus = chainParams->GetConsensus();

    // hash genesis is correct
    BOOST_CHECK_EQUAL(consensus.hashGenesisBlock, chainParams->GenesisBlock().GetHash());

    // target timespan is an even multiple of spacing
    BOOST_CHECK_EQUAL(consensus.nPowTargetTimespan % consensus.nPowTargetSpacing, 0);

    // genesis nBits is positive, doesn't overflow and is lower than powLimit
    arith_uint256 pow_compact;
    bool neg, over;
    pow_compact.SetCompact(chainParams->GenesisBlock().nBits, &neg, &over);
    BOOST_CHECK(!neg && pow_compact != 0);
    BOOST_CHECK(!over);
    BOOST_CHECK(UintToArith256(consensus.powLimit) >= pow_compact);

    // The legacy retarget (GetNextWorkRequired_V1) multiplies a target by up to
    // four target timespans, after halving targets of more than 235 bits.
    // Check that this can't overflow. (Dark Gravity Wave and ASERT compute
    // with arbitrary precision.)
    if (!consensus.fPowNoRetargeting && !consensus.fPowAlwaysAsert) {
        arith_uint256 max_target{UintToArith256(consensus.powLimit)};
        if (max_target.bits() > 235) max_target >>= 1;
        const arith_uint256 max_factor{static_cast<uint64_t>(4 * std::max<int64_t>(consensus.nPowTargetTimespan, 88775))};
        BOOST_CHECK_LE(max_target.bits() + max_factor.bits(), 256U);
    }
}

BOOST_AUTO_TEST_CASE(ChainParams_MAIN_sanity)
{
    sanity_check_chainparams(*m_node.args, ChainType::MAIN);
}

BOOST_AUTO_TEST_CASE(ChainParams_REGTEST_sanity)
{
    sanity_check_chainparams(*m_node.args, ChainType::REGTEST);
}

BOOST_AUTO_TEST_CASE(ChainParams_TESTNET_sanity)
{
    sanity_check_chainparams(*m_node.args, ChainType::TESTNET);
}

BOOST_AUTO_TEST_CASE(ChainParams_TESTNET4_sanity)
{
    sanity_check_chainparams(*m_node.args, ChainType::TESTNET4);
}

BOOST_AUTO_TEST_CASE(ChainParams_SIGNET_sanity)
{
    sanity_check_chainparams(*m_node.args, ChainType::SIGNET);
}

BOOST_AUTO_TEST_CASE(RandomX_consensus_profile_scaffold)
{
    const auto profile = randomx::GetConsensusProfileV1();
    BOOST_CHECK_EQUAL(profile.upstream_repo, "tevador/RandomX");
    BOOST_CHECK_EQUAL(profile.upstream_tag, "v2.0");
    BOOST_CHECK_EQUAL(profile.upstream_commit, "e0db3c4a8de36d77f50c12f7099bc37401cab88c");
    BOOST_CHECK(randomx::IsConsensusProfilePinned(profile));

    BOOST_CHECK(randomx::ValidateScaffoldFlags(randomx::FLAG_V2).empty());
    BOOST_CHECK_EQUAL(
        randomx::ValidateScaffoldFlags(randomx::FLAG_DEFAULT),
        "RandomX v2 flag is required by consensus profile");
}

BOOST_AUTO_TEST_CASE(regtest_keeps_difficulty)
{
    const auto chain_params = CreateChainParams(*m_node.args, ChainType::REGTEST);
    const auto& consensus = chain_params->GetConsensus();
    BOOST_CHECK(consensus.fPowNoRetargeting);
    BOOST_CHECK_EQUAL(chain_params->GenesisBlock().nBits, UintToArith256(consensus.powLimit).GetCompact());

    // Even at a retarget boundary, regtest keeps the previous block's difficulty.
    CBlockIndex prev;
    prev.nHeight = 2015;
    prev.nTime = 1388590627;
    prev.nBits = 0x207fffff;
    CBlockHeader next;
    next.nTime = prev.nTime + 1;
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&prev, &next, consensus), 0x207fffffU);
}

BOOST_AUTO_TEST_CASE(ChainParams_MARSQNET_params)
{
    const auto params = CreateChainParams(*m_node.args, ChainType::MARSQNET);
    const auto& consensus = params->GetConsensus();
    BOOST_CHECK(params->GetChainType() == ChainType::MARSQNET);
    BOOST_CHECK_EQUAL(params->GetChainTypeString(), "marsqnet");
    BOOST_CHECK(consensus.fPowUseRandomX);
    BOOST_CHECK(consensus.fPowAlwaysAsert);
    BOOST_CHECK(!consensus.fPowNoRetargeting);
    BOOST_CHECK(!consensus.fPowAllowMinDifficultyBlocks);
    BOOST_CHECK(consensus.fStrictChainId);
    BOOST_CHECK_EQUAL(consensus.nAuxpowChainId, 0x4D51);
    BOOST_CHECK_EQUAL(consensus.nABWLActivationHeight, 1);
    BOOST_CHECK(consensus.IsPQWitnessActive(1));
    BOOST_CHECK_EQUAL(consensus.nSubsidyHalvingInterval, 395699);
    BOOST_CHECK_EQUAL(params->GetDefaultPort(), 29338);
    BOOST_CHECK_EQUAL(params->Bech32HRP(), "mqt");
    BOOST_CHECK_EQUAL(params->GenesisBlock().nBits, UintToArith256(consensus.powLimit).GetCompact());
    BOOST_CHECK(params->GetAvailableSnapshotHeights().empty());

    // Plain regtest no longer uses RandomX.
    BOOST_CHECK(!CreateChainParams(*m_node.args, ChainType::REGTEST)->GetConsensus().fPowUseRandomX);

    // -chain=marsqnet, -marsqnet and the deprecated qdevnet spellings select marsqnet.
    BOOST_CHECK(ChainTypeFromString("marsqnet") == ChainType::MARSQNET);
    for (const auto& [arg, value] : std::vector<std::pair<std::string, std::string>>{{"-chain", "marsqnet"}, {"-chain", "qdevnet"}, {"-marsqnet", "1"}, {"-qdevnet", "1"}}) {
        ArgsManager args;
        args.ForceSetArg(arg, value);
        BOOST_CHECK(args.GetChainType() == ChainType::MARSQNET);
    }
}

BOOST_AUTO_TEST_CASE(marsqnet_asert_retargets)
{
    const auto params = CreateChainParams(*m_node.args, ChainType::MARSQNET);
    const auto& consensus = params->GetConsensus();
    const arith_uint256 pow_limit{UintToArith256(consensus.powLimit)};
    const uint32_t limit_bits{pow_limit.GetCompact()};

    // Block 1, the ASERT anchor, is mined at the proof-of-work limit.
    std::vector<CBlockIndex> blocks(102);
    blocks[0].nHeight = 0;
    blocks[0].nTime = params->GenesisBlock().nTime;
    blocks[0].nBits = limit_bits;
    CBlockHeader next;
    next.nTime = blocks[0].nTime + 1;
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&blocks[0], &next, consensus), limit_bits);

    const auto build_chain = [&](int64_t spacing) {
        for (size_t i = 1; i < blocks.size(); ++i) {
            blocks[i].pprev = &blocks[i - 1];
            blocks[i].nHeight = i;
            blocks[i].nTime = blocks[0].nTime + 1000 + (i - 1) * spacing;
            blocks[i].nBits = limit_bits;
        }
        next.nTime = blocks.back().nTime + spacing;
        arith_uint256 target;
        target.SetCompact(GetNextWorkRequired(&blocks.back(), &next, consensus));
        return target;
    };

    // Blocks far faster than the 123-second target make the next block harder.
    BOOST_CHECK(build_chain(1) < pow_limit / 2);
    // Slow blocks never go easier than the limit.
    BOOST_CHECK(build_chain(1000) == arith_uint256().SetCompact(limit_bits));
}

#ifdef ENABLE_RANDOMX_VENDOR
BOOST_AUTO_TEST_CASE(marsqnet_genesis_randomx_pow)
{
    const auto params = CreateChainParams(*m_node.args, ChainType::MARSQNET);
    const CBlockHeader header = params->GenesisBlock().GetBlockHeader();
    BOOST_CHECK(CheckProofOfWork(header, header.nBits, params->GetConsensus()));
}
#endif

BOOST_AUTO_TEST_CASE(RandomX_pow_fails_closed)
{
    const auto params = CreateChainParams(*m_node.args, ChainType::MARSQNET);
    const auto& consensus = params->GetConsensus();
    const CBlockHeader header = params->GenesisBlock().GetBlockHeader();
    const unsigned int easiest_bits = UintToArith256(consensus.powLimit).GetCompact();

    // A zero hash meets any valid target, so a failed hash must never become one.
    BOOST_CHECK(CheckProofOfWork(uint256{}, easiest_bits, consensus));

#ifdef ENABLE_RANDOMX_VENDOR
    BOOST_CHECK(IsProofOfWorkSupported(consensus));
    const auto hash = GetProofOfWorkHash(header, consensus);
    BOOST_REQUIRE(hash.has_value());
    BOOST_CHECK(!hash->IsNull());
    BOOST_CHECK(*hash != header.GetPoWHash());
    BOOST_CHECK_EQUAL(CheckProofOfWork(header, easiest_bits, consensus), CheckProofOfWork(*hash, easiest_bits, consensus));
#else
    BOOST_CHECK(!IsProofOfWorkSupported(consensus));
    BOOST_CHECK(!GetProofOfWorkHash(header, consensus).has_value());
    BOOST_CHECK(!CheckProofOfWork(header, easiest_bits, consensus));
#endif
}

BOOST_AUTO_TEST_CASE(Scrypt_pow_hash_unchanged)
{
    const auto params = CreateChainParams(*m_node.args, ChainType::MAIN);
    BOOST_CHECK(IsProofOfWorkSupported(params->GetConsensus()));
    const CBlockHeader header = params->GenesisBlock().GetBlockHeader();
    const auto hash = GetProofOfWorkHash(header, params->GetConsensus());
    BOOST_REQUIRE(hash.has_value());
    BOOST_CHECK_EQUAL(*hash, header.GetPoWHash());
}

#ifdef ENABLE_RANDOMX_VENDOR
BOOST_AUTO_TEST_CASE(RandomX_wrapper_deterministic_vectors)
{
    randomx::CacheHandle cache;
    std::string error;

    const std::vector<unsigned char> key_1{'m','a','r','s','-','s','e','e','d','-','e','p','o','c','h','-','0','0','0','1'};
    BOOST_CHECK(randomx::InitCache(cache, Span<const unsigned char>(key_1.data(), key_1.size()), error));
    BOOST_CHECK(error.empty());

    std::array<unsigned char, 32> hash_1a;
    std::array<unsigned char, 32> hash_1b;
    const std::vector<unsigned char> input_1{'b','l','o','c','k','-','h','e','a','d','e','r','-','s','a','m','p','l','e','-','0','0','0','1'};
    BOOST_CHECK(randomx::HashOnce(cache, Span<const unsigned char>(input_1.data(), input_1.size()), hash_1a, error));
    BOOST_CHECK(error.empty());
    BOOST_CHECK(randomx::HashOnce(cache, Span<const unsigned char>(input_1.data(), input_1.size()), hash_1b, error));
    BOOST_CHECK(error.empty());
    BOOST_CHECK_EQUAL(HexStr(hash_1a), HexStr(hash_1b));

    const std::vector<unsigned char> key_2{'m','a','r','s','-','s','e','e','d','-','e','p','o','c','h','-','0','0','0','2'};
    BOOST_CHECK(randomx::InitCache(cache, Span<const unsigned char>(key_2.data(), key_2.size()), error));
    BOOST_CHECK(error.empty());

    std::array<unsigned char, 32> hash_2;
    const std::vector<unsigned char> input_2{'b','l','o','c','k','-','h','e','a','d','e','r','-','s','a','m','p','l','e','-','0','0','0','2'};
    BOOST_CHECK(randomx::HashOnce(cache, Span<const unsigned char>(input_2.data(), input_2.size()), hash_2, error));
    BOOST_CHECK(error.empty());
    BOOST_CHECK_NE(HexStr(hash_1a), HexStr(hash_2));

    randomx::CacheHandle empty_cache;
    std::array<unsigned char, 32> hash_fail;
    BOOST_CHECK(!randomx::HashOnce(empty_cache, Span<const unsigned char>(input_1.data(), input_1.size()), hash_fail, error));
    BOOST_CHECK_EQUAL(error, "RandomX cache is not initialized");

    randomx::ReleaseCache(cache);
}
#endif

BOOST_AUTO_TEST_SUITE_END()
