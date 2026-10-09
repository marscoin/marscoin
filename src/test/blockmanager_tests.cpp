// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <clientversion.h>
#include <node/blockstorage.h>
#include <node/context.h>
#include <node/kernel_notifications.h>
#include <script/solver.h>
#include <primitives/block.h>
#include <util/chaintype.h>
#include <validation.h>

#include <boost/test/unit_test.hpp>
#include <test/util/logging.h>
#include <test/util/setup_common.h>

using node::BLOCK_SERIALIZATION_HEADER_SIZE;
using node::BlockManager;
using node::KernelNotifications;
using node::MAX_BLOCKFILE_SIZE;

// use BasicTestingSetup here for the data directory configuration, setup, and cleanup
BOOST_FIXTURE_TEST_SUITE(blockmanager_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(blockmanager_find_block_pos)
{
    const auto params {CreateChainParams(ArgsManager{}, ChainType::MAIN)};
    KernelNotifications notifications{*Assert(m_node.shutdown), m_node.exit_status, *Assert(m_node.warnings)};
    const BlockManager::Options blockman_opts{
        .chainparams = *params,
        .blocks_dir = m_args.GetBlocksDirPath(),
        .notifications = notifications,
    };
    BlockManager blockman{*Assert(m_node.shutdown), blockman_opts};
    // simulate adding a genesis block normally
    BOOST_CHECK_EQUAL(blockman.SaveBlockToDisk(params->GenesisBlock(), 0).nPos, BLOCK_SERIALIZATION_HEADER_SIZE);
    // simulate what happens during reindex
    // simulate a well-formed genesis block being found at offset 8 in the blk00000.dat file
    // the block is found at offset 8 because there is an 8 byte serialization header
    // consisting of 4 magic bytes + 4 length bytes before each block in a well-formed blk file.
    const FlatFilePos pos{0, BLOCK_SERIALIZATION_HEADER_SIZE};
    blockman.UpdateBlockInfo(params->GenesisBlock(), 0, pos);
    // now simulate what happens after reindex for the first new block processed
    // the actual block contents don't matter, just that it's a block.
    // verify that the write position is at offset 0x12d.
    // this is a check to make sure that https://github.com/bitcoin/bitcoin/issues/21379 does not recur
    // 8 bytes (for serialization header) + 285 (for serialized genesis block) = 293
    // add another 8 bytes for the second block's serialization header and we get 293 + 8 = 301
    FlatFilePos actual{blockman.SaveBlockToDisk(params->GenesisBlock(), 1)};
    BOOST_CHECK_EQUAL(actual.nPos, BLOCK_SERIALIZATION_HEADER_SIZE + ::GetSerializeSize(TX_WITH_WITNESS(params->GenesisBlock())) + BLOCK_SERIALIZATION_HEADER_SIZE);
}

BOOST_FIXTURE_TEST_CASE(blockmanager_scan_unlink_already_pruned_files, TestChain100Setup)
{
    // Cap last block file size, and mine new block in a new block file.
    const auto& chainman = Assert(m_node.chainman);
    auto& blockman = chainman->m_blockman;
    const CBlockIndex* old_tip{WITH_LOCK(chainman->GetMutex(), return chainman->ActiveChain().Tip())};
    WITH_LOCK(chainman->GetMutex(), blockman.GetBlockFileInfo(old_tip->GetBlockPos().nFile)->nSize = MAX_BLOCKFILE_SIZE);
    CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));

    // Prune the older block file, but don't unlink it
    int file_number;
    {
        LOCK(chainman->GetMutex());
        file_number = old_tip->GetBlockPos().nFile;
        blockman.PruneOneBlockFile(file_number);
    }

    const FlatFilePos pos(file_number, 0);

    // Check that the file is not unlinked after ScanAndUnlinkAlreadyPrunedFiles
    // if m_have_pruned is not yet set
    WITH_LOCK(chainman->GetMutex(), blockman.ScanAndUnlinkAlreadyPrunedFiles());
    BOOST_CHECK(!blockman.OpenBlockFile(pos, true).IsNull());

    // Check that the file is unlinked after ScanAndUnlinkAlreadyPrunedFiles
    // once m_have_pruned is set
    blockman.m_have_pruned = true;
    WITH_LOCK(chainman->GetMutex(), blockman.ScanAndUnlinkAlreadyPrunedFiles());
    BOOST_CHECK(blockman.OpenBlockFile(pos, true).IsNull());

    // Check that calling with already pruned files doesn't cause an error
    WITH_LOCK(chainman->GetMutex(), blockman.ScanAndUnlinkAlreadyPrunedFiles());

    // Check that the new tip file has not been removed
    const CBlockIndex* new_tip{WITH_LOCK(chainman->GetMutex(), return chainman->ActiveChain().Tip())};
    BOOST_CHECK_NE(old_tip, new_tip);
    const int new_file_number{WITH_LOCK(chainman->GetMutex(), return new_tip->GetBlockPos().nFile)};
    const FlatFilePos new_pos(new_file_number, 0);
    BOOST_CHECK(!blockman.OpenBlockFile(new_pos, true).IsNull());
}

BOOST_FIXTURE_TEST_CASE(blockmanager_block_data_availability, TestChain100Setup)
{
    // The goal of the function is to return the first not pruned block in the range [upper_block, lower_block].
    LOCK(::cs_main);
    auto& chainman = m_node.chainman;
    auto& blockman = chainman->m_blockman;
    const CBlockIndex& tip = *chainman->ActiveTip();

    // Function to prune all blocks from 'last_pruned_block' down to the genesis block
    const auto& func_prune_blocks = [&](CBlockIndex* last_pruned_block)
    {
        LOCK(::cs_main);
        CBlockIndex* it = last_pruned_block;
        while (it != nullptr && it->nStatus & BLOCK_HAVE_DATA) {
            it->nStatus &= ~BLOCK_HAVE_DATA;
            it = it->pprev;
        }
    };

    // 1) Return genesis block when all blocks are available
    BOOST_CHECK_EQUAL(blockman.GetFirstBlock(tip, BLOCK_HAVE_DATA), chainman->ActiveChain()[0]);
    BOOST_CHECK(blockman.CheckBlockDataAvailability(tip, *chainman->ActiveChain()[0]));

    // 2) Check lower_block when all blocks are available
    CBlockIndex* lower_block = chainman->ActiveChain()[tip.nHeight / 2];
    BOOST_CHECK(blockman.CheckBlockDataAvailability(tip, *lower_block));

    // Prune half of the blocks
    int height_to_prune = tip.nHeight / 2;
    CBlockIndex* first_available_block = chainman->ActiveChain()[height_to_prune + 1];
    CBlockIndex* last_pruned_block = first_available_block->pprev;
    func_prune_blocks(last_pruned_block);

    // 3) The last block not pruned is in-between upper-block and the genesis block
    BOOST_CHECK_EQUAL(blockman.GetFirstBlock(tip, BLOCK_HAVE_DATA), first_available_block);
    BOOST_CHECK(blockman.CheckBlockDataAvailability(tip, *first_available_block));
    BOOST_CHECK(!blockman.CheckBlockDataAvailability(tip, *last_pruned_block));
}

BOOST_AUTO_TEST_CASE(blockmanager_flush_block_file)
{
    KernelNotifications notifications{*Assert(m_node.shutdown), m_node.exit_status, *Assert(m_node.warnings)};
    node::BlockManager::Options blockman_opts{
        .chainparams = Params(),
        .blocks_dir = m_args.GetBlocksDirPath(),
        .notifications = notifications,
    };
    BlockManager blockman{*Assert(m_node.shutdown), blockman_opts};

    // Test blocks with no transactions, not even a coinbase
    CBlock block1;
    block1.nVersion = 1;
    CBlock block2;
    block2.nVersion = 2;
    CBlock block3;
    block3.nVersion = 3;

    // They are 80 bytes header + 1 byte 0x00 for vtx length
    constexpr int TEST_BLOCK_SIZE{81};

    // Blockstore is empty
    BOOST_CHECK_EQUAL(blockman.CalculateCurrentUsage(), 0);

    // Write the first block to a new location.
    FlatFilePos pos1{blockman.SaveBlockToDisk(block1, /*nHeight=*/1)};

    // Write second block
    FlatFilePos pos2{blockman.SaveBlockToDisk(block2, /*nHeight=*/2)};

    // Two blocks in the file
    BOOST_CHECK_EQUAL(blockman.CalculateCurrentUsage(), (TEST_BLOCK_SIZE + BLOCK_SERIALIZATION_HEADER_SIZE) * 2);

    // First two blocks are written as expected
    // Errors are expected because block data is junk, thrown AFTER successful read
    CBlock read_block;
    BOOST_CHECK_EQUAL(read_block.nVersion, 0);
    {
        ASSERT_DEBUG_LOG("ReadBlockFromDisk: Errors in block header");
        BOOST_CHECK(!blockman.ReadBlockFromDisk(read_block, pos1));
        BOOST_CHECK_EQUAL(read_block.nVersion, 1);
    }
    {
        ASSERT_DEBUG_LOG("ReadBlockFromDisk: Errors in block header");
        BOOST_CHECK(!blockman.ReadBlockFromDisk(read_block, pos2));
        BOOST_CHECK_EQUAL(read_block.nVersion, 2);
    }

    // During reindex, the flat file block storage will not be written to.
    // UpdateBlockInfo will, however, update the blockfile metadata.
    // Verify this behavior by attempting (and failing) to write block 3 data
    // to block 2 location.
    CBlockFileInfo* block_data = blockman.GetBlockFileInfo(0);
    BOOST_CHECK_EQUAL(block_data->nBlocks, 2);
    blockman.UpdateBlockInfo(block3, /*nHeight=*/3, /*pos=*/pos2);
    // Metadata is updated...
    BOOST_CHECK_EQUAL(block_data->nBlocks, 3);
    // ...but there are still only two blocks in the file
    BOOST_CHECK_EQUAL(blockman.CalculateCurrentUsage(), (TEST_BLOCK_SIZE + BLOCK_SERIALIZATION_HEADER_SIZE) * 2);

    // Block 2 was not overwritten:
    blockman.ReadBlockFromDisk(read_block, pos2);
    BOOST_CHECK_EQUAL(read_block.nVersion, 2);
}

BOOST_AUTO_TEST_CASE(abwl_state_persists_outside_block_index_record)
{
    CBlockHeader header_abwl;
    header_abwl.nVersion = 1;
    header_abwl.nTime = 1;
    header_abwl.nBits = 0x207fffff;
    header_abwl.nNonce = 1;
    CBlockHeader header_legacy{header_abwl};
    header_legacy.nNonce = 2;
    const uint256 hash_abwl{header_abwl.GetHash()};
    const uint256 hash_legacy{header_legacy.GetHash()};

    // A block on an ABWL chain carries state; a mainnet block does not.
    CBlockIndex abwl_index{header_abwl};
    abwl_index.phashBlock = &hash_abwl;
    abwl_index.nHeight = 1;
    abwl_index.nBlockWeight = 1234;
    abwl_index.nABWL_epsilon = 2'000'001;
    abwl_index.nABWL_beta = 2'000'002;
    CBlockIndex legacy_index{header_legacy};
    legacy_index.phashBlock = &hash_legacy;
    legacy_index.nHeight = 1;
    legacy_index.nBlockWeight = 999;

    // The block index record keeps the upstream format: ABWL fields don't change its bytes.
    CBlockIndex abwl_index_without_state{header_abwl};
    abwl_index_without_state.phashBlock = &hash_abwl;
    abwl_index_without_state.nHeight = 1;
    DataStream with_state, without_state;
    with_state << CDiskBlockIndex{&abwl_index};
    without_state << CDiskBlockIndex{&abwl_index_without_state};
    BOOST_CHECK(with_state.str() == without_state.str());

    kernel::BlockTreeDB db{DBParams{.path = m_args.GetDataDirNet() / "abwl_index", .cache_bytes = 1 << 20, .memory_only = true}};
    BOOST_REQUIRE(db.WriteBatchSync({}, 0, {&abwl_index, &legacy_index}));

    std::map<uint256, CBlockIndex> loaded;
    const auto insert{[&](const uint256& hash) -> CBlockIndex* {
        if (hash.IsNull()) return nullptr;
        auto [it, inserted] = loaded.try_emplace(hash);
        it->second.phashBlock = &it->first;
        return &it->second;
    }};
    LOCK(cs_main);
    BOOST_REQUIRE(db.LoadBlockIndexGuts(Params().GetConsensus(), insert, m_interrupt));
    BOOST_REQUIRE_EQUAL(loaded.size(), 2U);

    const CBlockIndex& loaded_abwl{loaded.at(hash_abwl)};
    BOOST_CHECK_EQUAL(loaded_abwl.nBlockWeight, 1234);
    BOOST_CHECK_EQUAL(loaded_abwl.nABWL_epsilon, 2'000'001);
    BOOST_CHECK_EQUAL(loaded_abwl.nABWL_beta, 2'000'002);

    const CBlockIndex& loaded_legacy{loaded.at(hash_legacy)};
    BOOST_CHECK_EQUAL(loaded_legacy.nABWL_epsilon, 0);
    BOOST_CHECK_EQUAL(loaded_legacy.nABWL_beta, 0);
}

BOOST_AUTO_TEST_SUITE_END()
