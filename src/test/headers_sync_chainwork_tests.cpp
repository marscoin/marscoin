// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <config/bitcoin-config.h> // IWYU pragma: keep

#include <arith_uint256.h>
#include <chain.h>
#include <chainparams.h>
#include <consensus/params.h>
#include <headerssync.h>
#include <pow.h>
#include <test/util/setup_common.h>
#include <validation.h>
#include <vector>

#include <boost/test/unit_test.hpp>

struct HeadersGeneratorSetup : public RegTestingSetup {
    /** Search for a nonce to meet (regtest) proof of work */
    void FindProofOfWork(CBlockHeader& starting_header);
    /**
     * Generate headers in a chain that build off a given starting hash, using
     * the given nVersion, advancing time by 1 second from the starting
     * prev_time, and with a fixed merkle root hash.
     */
    void GenerateHeaders(std::vector<CBlockHeader>& headers, size_t count,
            const uint256& starting_hash, const int nVersion, int prev_time,
            const uint256& merkle_root, const uint32_t nBits);
};

void HeadersGeneratorSetup::FindProofOfWork(CBlockHeader& starting_header)
{
    while (!CheckProofOfWork(starting_header, starting_header.nBits, Params().GetConsensus())) {
        ++(starting_header.nNonce);
    }
}

void HeadersGeneratorSetup::GenerateHeaders(std::vector<CBlockHeader>& headers,
        size_t count, const uint256& starting_hash, const int nVersion, int prev_time,
        const uint256& merkle_root, const uint32_t nBits)
{
    uint256 prev_hash = starting_hash;

    while (headers.size() < count) {
        headers.emplace_back();
        CBlockHeader& next_header = headers.back();;
        next_header.nVersion = nVersion;
        next_header.hashPrevBlock = prev_hash;
        next_header.hashMerkleRoot = merkle_root;
        next_header.nTime = prev_time+1;
        next_header.nBits = nBits;

        FindProofOfWork(next_header);
        prev_hash = next_header.GetHash();
        prev_time = next_header.nTime;
    }
    return;
}

BOOST_FIXTURE_TEST_SUITE(headers_sync_chainwork_tests, HeadersGeneratorSetup)

// In this test, we construct two sets of headers from genesis, one with
// sufficient proof of work and one without.
// 1. We deliver the first set of headers and verify that the headers sync state
//    updates to the REDOWNLOAD phase successfully.
// 2. Then we deliver the second set of headers and verify that they fail
//    processing (presumably due to commitments not matching).
// 3. Finally, we verify that repeating with the first set of headers in both
//    phases is successful.
BOOST_AUTO_TEST_CASE(headers_sync_state)
{
    std::vector<CBlockHeader> first_chain;
    std::vector<CBlockHeader> second_chain;

    std::unique_ptr<HeadersSyncState> hss;

    const int target_blocks = 15000;
    arith_uint256 chain_work = target_blocks*2;

    // Generate headers for two different chains (using differing merkle roots
    // to ensure the headers are different).
    GenerateHeaders(first_chain, target_blocks-1, Params().GenesisBlock().GetHash(),
            Params().GenesisBlock().nVersion, Params().GenesisBlock().nTime,
            ArithToUint256(0), Params().GenesisBlock().nBits);

    GenerateHeaders(second_chain, target_blocks-2, Params().GenesisBlock().GetHash(),
            Params().GenesisBlock().nVersion, Params().GenesisBlock().nTime,
            ArithToUint256(1), Params().GenesisBlock().nBits);

    const CBlockIndex* chain_start = WITH_LOCK(::cs_main, return m_node.chainman->m_blockman.LookupBlockIndex(Params().GenesisBlock().GetHash()));
    std::vector<CBlockHeader> headers_batch;

    // Feed the first chain to HeadersSyncState, by delivering 1 header
    // initially and then the rest.
    headers_batch.insert(headers_batch.end(), std::next(first_chain.begin()), first_chain.end());

    hss.reset(new HeadersSyncState(0, Params().GetConsensus(), chain_start, chain_work));
    (void)hss->ProcessNextHeaders({first_chain.front()}, true);
    // Pretend the first header is still "full", so we don't abort.
    auto result = hss->ProcessNextHeaders(headers_batch, true);

    // This chain should look valid, and we should have met the proof-of-work
    // requirement.
    BOOST_CHECK(result.success);
    BOOST_CHECK(result.request_more);
    BOOST_CHECK(hss->GetState() == HeadersSyncState::State::REDOWNLOAD);

    // Try to sneakily feed back the second chain.
    result = hss->ProcessNextHeaders(second_chain, true);
    BOOST_CHECK(!result.success); // foiled!
    BOOST_CHECK(hss->GetState() == HeadersSyncState::State::FINAL);

    // Now try again, this time feeding the first chain twice.
    hss.reset(new HeadersSyncState(0, Params().GetConsensus(), chain_start, chain_work));
    (void)hss->ProcessNextHeaders(first_chain, true);
    BOOST_CHECK(hss->GetState() == HeadersSyncState::State::REDOWNLOAD);

    result = hss->ProcessNextHeaders(first_chain, true);
    BOOST_CHECK(result.success);
    BOOST_CHECK(!result.request_more);
    // All headers should be ready for acceptance:
    BOOST_CHECK(result.pow_validated_headers.size() == first_chain.size());
    // Nothing left for the sync logic to do:
    BOOST_CHECK(hss->GetState() == HeadersSyncState::State::FINAL);

    // Finally, verify that just trying to process the second chain would not
    // succeed (too little work)
    hss.reset(new HeadersSyncState(0, Params().GetConsensus(), chain_start, chain_work));
    BOOST_CHECK(hss->GetState() == HeadersSyncState::State::PRESYNC);
     // Pretend just the first message is "full", so we don't abort.
    (void)hss->ProcessNextHeaders({second_chain.front()}, true);
    BOOST_CHECK(hss->GetState() == HeadersSyncState::State::PRESYNC);

    headers_batch.clear();
    headers_batch.insert(headers_batch.end(), std::next(second_chain.begin(), 1), second_chain.end());
    // Tell the sync logic that the headers message was not full, implying no
    // more headers can be requested. For a low-work-chain, this should causes
    // the sync to end with no headers for acceptance.
    result = hss->ProcessNextHeaders(headers_batch, false);
    BOOST_CHECK(hss->GetState() == HeadersSyncState::State::FINAL);
    BOOST_CHECK(result.pow_validated_headers.empty());
    BOOST_CHECK(!result.request_more);
    // Nevertheless, no validation errors should have been detected with the
    // chain:
    BOOST_CHECK(result.success);
}

#ifdef ENABLE_RANDOMX_VENDOR
BOOST_AUTO_TEST_CASE(headers_presync_randomx_key_binding)
{
    // The presync phase checks RandomX proof of work itself, keyed by seed
    // blocks it remembers as they go by (doc/quantum-randomx-key-binding-v1.md).
    // Test parameters: an easy target, key binding from height 5, epoch 16,
    // lag 4, so 60 headers cross three key changes.
    Consensus::Params params{Params().GetConsensus()};
    params.fPowUseRandomX = true;
    params.powLimit = uint256{"7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
    params.nRandomXKeyBindingHeight = 5;
    params.nRandomXSeedEpoch = 16;
    params.nRandomXSeedLag = 4;
    const uint32_t bits{UintToArith256(params.powLimit).GetCompact()};
    const uint256 genesis_hash{Params().GenesisBlock().GetHash()};

    std::vector<CBlockHeader> chain;
    auto key_for = [&](int height, const CBlockHeader& header) {
        if (!params.IsRandomXKeyBound(height)) return header.hashPrevBlock;
        const int seed{RandomXSeedHeight(height, params)};
        return seed == 0 ? genesis_hash : chain[seed - 1].GetHash();
    };
    auto grind = [&](CBlockHeader& header, const uint256& key) {
        while (!CheckProofOfWorkWithKey(header, header.nBits, key, params)) ++header.nNonce;
    };
    uint256 prev{genesis_hash};
    uint32_t time{Params().GenesisBlock().nTime};
    for (int height = 1; height <= 60; ++height) {
        CBlockHeader header;
        header.nVersion = 4 | (params.nAuxpowChainId << 16);
        header.hashPrevBlock = prev;
        header.nTime = ++time;
        header.nBits = bits;
        grind(header, key_for(height, header));
        chain.push_back(header);
        prev = header.GetHash();
    }

    const CBlockIndex* chain_start = WITH_LOCK(::cs_main, return m_node.chainman->m_blockman.LookupBlockIndex(genesis_hash));
    const arith_uint256 unreachable_work{arith_uint256{1} << 200};

    // A valid chain stays in presync and asks for more.
    {
        HeadersSyncState hss{0, params, chain_start, unreachable_work};
        const auto result{hss.ProcessNextHeaders(chain, true)};
        BOOST_CHECK(result.success);
        BOOST_CHECK(result.request_more);
        BOOST_CHECK(hss.GetState() == HeadersSyncState::State::PRESYNC);
    }

    // A header ground with the wrong key (here the null key) is rejected, even
    // though it would pass with that key.
    {
        std::vector<CBlockHeader> bad{chain.begin(), chain.begin() + 39};
        CBlockHeader header{chain[39]};
        while (!(CheckProofOfWorkWithKey(header, header.nBits, uint256{}, params) &&
                 !CheckProofOfWorkWithKey(header, header.nBits, key_for(40, header), params))) {
            ++header.nNonce;
        }
        bad.push_back(header);
        HeadersSyncState hss{0, params, chain_start, unreachable_work};
        BOOST_CHECK(!hss.ProcessNextHeaders(bad, true).success);
    }

    // A merged-mined header after key binding is rejected.
    {
        std::vector<CBlockHeader> bad{chain.begin(), chain.begin() + 30};
        bad.back().SetAuxpowVersion(true);
        HeadersSyncState hss{0, params, chain_start, unreachable_work};
        BOOST_CHECK(!hss.ProcessNextHeaders(bad, true).success);
    }
}
#endif

BOOST_AUTO_TEST_SUITE_END()
