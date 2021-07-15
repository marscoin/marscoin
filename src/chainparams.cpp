// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"

#include "random.h"
#include "util.h"
#include "utilstrencodings.h"

#include <assert.h>

#include <boost/assign/list_of.hpp>

using namespace std;
using namespace boost::assign;

struct SeedSpec6 {
    uint8_t addr[16];
    uint16_t port;
};

#include "chainparamsseeds.h"

/**
 * Main network
 */

//! Convert the pnSeeds6 array into usable address objects.
static void convertSeed6(std::vector<CAddress> &vSeedsOut, const SeedSpec6 *data, unsigned int count)
{
    // It'll only connect to one or two seed nodes because once it connects,
    // it'll get a pile of addresses with newer timestamps.
    // Seed nodes are given a random 'last seen time' of between one and two
    // weeks ago.
    const int64_t nOneWeek = 7*24*60*60;
    for (unsigned int i = 0; i < count; i++)
    {
        struct in6_addr ip;
        memcpy(&ip, data[i].addr, sizeof(ip));
        CAddress addr(CService(ip, data[i].port));
        addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
        vSeedsOut.push_back(addr);
    }
}

/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */
static Checkpoints::MapCheckpoints mapCheckpoints =
        boost::assign::map_list_of
        (         0, uint256("0x06e005f86644f15d2e4c62b59a038c798a3b0816ba58dcc8c91e02ce5a685299"))
        (      7000, uint256("0xb31738f115c3f44f7ecc500d2cac0afeedada59e8ef99386048c2b25ea455eba"))
        (     14000, uint256("0x137925f088b84170325cb065f7307814dfde8c2a8ad19a4cf545499517be4cf2"))
        (     21000, uint256("0x6d80d26ce1f5179f90277626fea1d19f6b9b3fd3f454c485b3ff2b10ff2d664b"))
        (     40000, uint256("0xa6994674aeb00faed6c8e77049798e846129a79676a55b20a6bdc4e182dc1b3a"))
        (     65000, uint256("0xc4737fe4f656e1504b4e939288c013bd26659321cd54bcf8c0b16e10f214a595"))
        (     71000, uint256("0x7f95786d37b0870ad9df518176f3c38592daa823668bd2a1d5229ca16bd43c18"))
        (     75000, uint256("0x25714a86429c02a702f4dc381fe26903fb27c54ab395d6c9b7f07a5610d06ac6"))
        (     91000, uint256("0xd192b6a59988b9d4629b5ff8e5de5589b8caa9feb5f2a70b1701be585861ab20"))
        (    110000, uint256("0x58b15f996a3af57d3230b104a6ba5b3702a0d5dedfb0a271b4bf76b5f22deb37"))
        (    120470, uint256("0x3e7ec1c3bd58d73fc145a56996ef3d031a98821dc536ae2c648b63c0f335435d"))
        (    120471, uint256("0x2897f7ee26bd4cc1fbbd55e626c737439658cfe00c3dcced50c2cc40af7e4184"))
        (    120522, uint256("0x633d27c5fb1df8cf98d4b2084cd4db1c747788a4d53ff1cfb802621e9e8899e3"))
        (    120622, uint256("0x77d2b34a0c2f0ee4eabe51add9844d8c4a1ac91f790b4f601f26948742bb5e01"))
        (    120623, uint256("0x6de582995302c262c51b1c92324b82dbf80926010e8b894df0e2d96a2a8dd8d8"))
        (    120624, uint256("0xdc55c2bea5f91e017c7740d2458c620b1eb91064f4812b8b830b2514189118e6"))
        (    120990, uint256("0x6700b33cc3b15c648dd204941984a569850ad9a5f8a994ff8513cd81972ab85d"))
        (    120998, uint256("0x3017a51571271c5632761f715de7a5c9ee8ea708a447725f1ccdc3321bf0dbdf"))
        (    120999, uint256("0x9e6a564906fc492875457466c095dcbf81e4480cb78e439626346640761d11ef"))
        (    121000, uint256("0x6cbf3996f30e47e6cfb8ba05c9c9aba5576bf0b12d88562f8a233c978f80a152"))
        (    121362, uint256("0x0877ae0dfbc5d0a339f3c6ee8b6b33cd620a5e4aa6237e96273a7647b2c6410d"))
        (    121363, uint256("0x411bf1bfdc754099836388a7dc3ed08d569f2d11b2decc96ed6e130b10ef547d"))
        (    121364, uint256("0xb25e2d746c8cbf98f3c5bc507c4fae48e0eaafa378778ef2c4fcc0083bcd955d"))
        (    121994, uint256("0xf1fc870f45046aafd0e8c8e7f4a74e40d02e84d79857049461efc3be28d40098"))
        (    123957, uint256("0xa3e8179fddd12346f0a8268ef533966b1d0a734953631919e731074c91fecf79"))
        (    124053, uint256("0x3aec5fb42854e278da56f74dd4317d0673405946f998b4c8307452e7c86f20ce"))
        (    940185, uint256("0x0d27d9da5fba6b093eff0f687308a97e48cfa2c9761fb91dcc78669412a22526"))
        (   1440000, uint256("0xcfc80ffcf364cc6a53091a3f1c4391ec97c4e76206d0eb13347a471b5640fb16"))
        
        ;
static const Checkpoints::CCheckpointData data = {
        &mapCheckpoints,
        1533511784, // * UNIX timestamp of last checkpoint block
        1097531,   // * total number of transactions between genesis and last checkpoint
                    //   (the tx=... number in the SetBestChain debug.log lines)
        100.0     // * estimated number of transactions per day after checkpoint
    };

static Checkpoints::MapCheckpoints mapCheckpointsTestnet =
        boost::assign::map_list_of
        ( 0, uint256("0x06e005f86644f15d2e4c62b59a038c798a3b0816ba58dcc8c91e02ce5a685299"))
        ;
static const Checkpoints::CCheckpointData dataTestnet = {
        &mapCheckpointsTestnet,
        1487715270,
        8731,
        576
    };

static Checkpoints::MapCheckpoints mapCheckpointsRegtest =
        boost::assign::map_list_of
        ( 0, uint256("0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206"))
        ;
static const Checkpoints::CCheckpointData dataRegtest = {
        &mapCheckpointsRegtest,
        0,
        0,
        0
    };

class CMainParams : public CChainParams {
public:
    CMainParams() {
        networkID = CBaseChainParams::MAIN;
        strNetworkID = "main";
        /** 
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 4-byte int at any alignment.
         */
        pchMessageStart[0] = 0xfb;
        pchMessageStart[1] = 0xc0;
        pchMessageStart[2] = 0xb6;
        pchMessageStart[3] = 0xdb;
        vAlertPubKey = ParseHex("0x");
        nDefaultPort = 8338;
        bnProofOfWorkLimit = ~uint256(0) >> 20;
        nSubsidyHalvingInterval = 395699; //668 Sols, every Mars year
        nEnforceBlockUpgradeMajority = 750;
        nRejectBlockOutdatedMajority = 950;
        nToCheckBlockUpgradeMajority = 1000;
        nMinerThreads = 0;
        nTargetTimespan = 3.5 * 24 * 60 * 60; // 1 mars day retarget diff
        nTargetSpacing = 2.5 * 60; // Marscoin: (2 Mars minutes)
        nMaxTipAge = 24 * 60 * 60;

        /**
         * Build the genesis block. Note that the output of the genesis coinbase cannot
         * be spent as it did not originally exist in the database.
         * 
         * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
         *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
         *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
         *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
         *   vMerkleTree: 4a5e1e
         */
        const char* pszTimestamp = "Forbes 30/Dec/2013 The Year of Bitcoin";
        CMutableTransaction txNew;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].nValue = 50 * COIN;
        txNew.vout[0].scriptPubKey = CScript() << ParseHex("0x") << OP_CHECKSIG;
        genesis.vtx.push_back(txNew);
        genesis.hashPrevBlock = 0;
        genesis.hashMerkleRoot = genesis.BuildMerkleTree();
        genesis.nVersion = 1;
        genesis.nTime    = 1388590627;
        genesis.nBits    = 0x1e0ffff0;
        genesis.nNonce   = 638933;

        hashGenesisBlock = genesis.GetHash();
        assert(hashGenesisBlock == uint256("0x06e005f86644f15d2e4c62b59a038c798a3b0816ba58dcc8c91e02ce5a685299"));
        assert(genesis.hashMerkleRoot == uint256("0xb9594f964ad5d42bd99edbfaaeeec900cd0f7563a14d90982cf6675df98d7863"));

        vSeeds.push_back(CDNSSeedData("marscoin.org", "dnsseed.marscoin.com"));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,50);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,5);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,178);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x88)(0xB2)(0x1E).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x88)(0xAD)(0xE4).convert_to_container<std::vector<unsigned char> >();

        convertSeed6(vFixedSeeds, pnSeed6_main, ARRAYLEN(pnSeed6_main));

        fRequireRPCPassword = true;
        fMiningRequiresPeers = true;
        fAllowMinDifficultyBlocks = false;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fSkipProofOfWorkCheck = false;
        fTestnetToBeDeprecatedFieldRPC = false;

        // Marscoin: Mainnet v2 enforced as of block 710k
        nEnforceV2AfterHeight = 1312374;
    }

    const Checkpoints::CCheckpointData& Checkpoints() const 
    {
        return data;
    }
};
static CMainParams mainParams;

/**
 * Testnet (v3)
 */
class CTestNetParams : public CMainParams {
public:
    CTestNetParams() {
        networkID = CBaseChainParams::TESTNET;
        strNetworkID = "test";
        pchMessageStart[0] = 0xfc;
        pchMessageStart[1] = 0xc1;
        pchMessageStart[2] = 0xb7;
        pchMessageStart[3] = 0xdc;
        vAlertPubKey = ParseHex("0x");
        nDefaultPort = 18337;
        nEnforceBlockUpgradeMajority = 51;
        nRejectBlockOutdatedMajority = 75;
        nToCheckBlockUpgradeMajority = 100;
        nMinerThreads = 0;
        nTargetTimespan = 3.5 * 24 * 60 * 60; // 1 mars day retarget diff
        nTargetSpacing = 2.5 * 60; // Marscoin: (2 Mars minutes)
        nMaxTipAge = 24 * 60 * 60;

        //! Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.nTime = 1388590627;
        genesis.nNonce = 638933;
        hashGenesisBlock = genesis.GetHash();
        assert(hashGenesisBlock == uint256("0x06e005f86644f15d2e4c62b59a038c798a3b0816ba58dcc8c91e02ce5a685299"));

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("marscoin.org", "testnet-seed.marscoin.org"));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY]     = std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x88)(0xB2)(0x1E).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x88)(0xAD)(0xE4).convert_to_container<std::vector<unsigned char> >();

        convertSeed6(vFixedSeeds, pnSeed6_test, ARRAYLEN(pnSeed6_test));

        fRequireRPCPassword = true;
        fMiningRequiresPeers = true;
        fAllowMinDifficultyBlocks = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = true;

        // Marscoin: v2 enforced using Bitcoin's supermajority rule
        nEnforceV2AfterHeight = -1;
    }
    const Checkpoints::CCheckpointData& Checkpoints() const 
    {
        return dataTestnet;
    }
};
static CTestNetParams testNetParams;

/**
 * Regression test
 */
class CRegTestParams : public CTestNetParams {
public:
    CRegTestParams() {
        networkID = CBaseChainParams::REGTEST;
        strNetworkID = "regtest";
        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        nSubsidyHalvingInterval = 150;
        nEnforceBlockUpgradeMajority = 750;
        nRejectBlockOutdatedMajority = 950;
        nToCheckBlockUpgradeMajority = 1000;
        nMinerThreads = 1;
        nTargetTimespan = 3.5 * 24 * 60 * 60; // 3.5 days
        nTargetSpacing = 2.5 * 60; // 2.5 minutes
        bnProofOfWorkLimit = ~uint256(0) >> 1;
        nMaxTipAge = 24 * 60 * 60;
        genesis.nTime = 1388590627;
        genesis.nBits = 0x1e0ffff0;
        genesis.nNonce = 638933;
        hashGenesisBlock = genesis.GetHash();
        //strprintf("Genesis Block (test)...%s)", hashGenesisBlock);
        nDefaultPort = 18447;
        assert(hashGenesisBlock == uint256("0x06e005f86644f15d2e4c62b59a038c798a3b0816ba58dcc8c91e02ce5a685299"));

        vFixedSeeds.clear(); //! Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();  //! Regtest mode doesn't have any DNS seeds.

        fRequireRPCPassword = false;
        fMiningRequiresPeers = false;
        fAllowMinDifficultyBlocks = true;
        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;
        fTestnetToBeDeprecatedFieldRPC = false;

        // Marscoin: v2 enforced using Bitcoin's supermajority rule
        nEnforceV2AfterHeight = -1;
    }
    const Checkpoints::CCheckpointData& Checkpoints() const 
    {
        return dataRegtest;
    }
};
static CRegTestParams regTestParams;

/**
 * Unit test
 */
class CUnitTestParams : public CMainParams, public CModifiableParams {
public:
    CUnitTestParams() {
        networkID = CBaseChainParams::UNITTEST;
        strNetworkID = "unittest";
        nDefaultPort = 18449;
        vFixedSeeds.clear(); //! Unit test mode doesn't have any fixed seeds.
        vSeeds.clear();  //! Unit test mode doesn't have any DNS seeds.

        fRequireRPCPassword = false;
        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = true;
        fAllowMinDifficultyBlocks = false;
        fMineBlocksOnDemand = true;

        // Marscoin: v2 enforced using Bitcoin's supermajority rule
        nEnforceV2AfterHeight = -1;
    }

    const Checkpoints::CCheckpointData& Checkpoints() const 
    {
        // UnitTest share the same checkpoints as MAIN
        return data;
    }

    //! Published setters to allow changing values in unit test cases
    virtual void setSubsidyHalvingInterval(int anSubsidyHalvingInterval)  { nSubsidyHalvingInterval=anSubsidyHalvingInterval; }
    virtual void setEnforceBlockUpgradeMajority(int anEnforceBlockUpgradeMajority)  { nEnforceBlockUpgradeMajority=anEnforceBlockUpgradeMajority; }
    virtual void setRejectBlockOutdatedMajority(int anRejectBlockOutdatedMajority)  { nRejectBlockOutdatedMajority=anRejectBlockOutdatedMajority; }
    virtual void setToCheckBlockUpgradeMajority(int anToCheckBlockUpgradeMajority)  { nToCheckBlockUpgradeMajority=anToCheckBlockUpgradeMajority; }
    virtual void setDefaultConsistencyChecks(bool afDefaultConsistencyChecks)  { fDefaultConsistencyChecks=afDefaultConsistencyChecks; }
    virtual void setAllowMinDifficultyBlocks(bool afAllowMinDifficultyBlocks) {  fAllowMinDifficultyBlocks=afAllowMinDifficultyBlocks; }
    virtual void setSkipProofOfWorkCheck(bool afSkipProofOfWorkCheck) { fSkipProofOfWorkCheck = afSkipProofOfWorkCheck; }
};
static CUnitTestParams unitTestParams;


static CChainParams *pCurrentParams = 0;

CModifiableParams *ModifiableParams()
{
   assert(pCurrentParams);
   assert(pCurrentParams==&unitTestParams);
   return (CModifiableParams*)&unitTestParams;
}

const CChainParams &Params() {
    assert(pCurrentParams);
    return *pCurrentParams;
}

CChainParams &Params(CBaseChainParams::Network network) {
    switch (network) {
        case CBaseChainParams::MAIN:
            return mainParams;
        case CBaseChainParams::TESTNET:
            return testNetParams;
        case CBaseChainParams::REGTEST:
            return regTestParams;
        case CBaseChainParams::UNITTEST:
            return unitTestParams;
        default:
            assert(false && "Unimplemented network");
            return mainParams;
    }
}

void SelectParams(CBaseChainParams::Network network) {
    SelectBaseParams(network);
    pCurrentParams = &Params(network);
}

bool SelectParamsFromCommandLine()
{
    CBaseChainParams::Network network = NetworkIdFromCommandLine();
    if (network == CBaseChainParams::MAX_NETWORK_TYPES)
        return false;

    SelectParams(network);
    return true;
}
