// Copyright (c) 2009-2019 The Bitcoin/Marscoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/assign/list_of.hpp> // for 'map_list_of()'
#include <boost/foreach.hpp>

#include "checkpoints.h"

#include "main.h"
#include "uint256.h"

namespace Checkpoints
{
    typedef std::map<int, uint256> MapCheckpoints;

    // How many times we expect transactions after the last checkpoint to
    // be slower. This number is a compromise, as it can't be accurate for
    // every system. When reindexing from a fast disk with a slow CPU, it
    // can be up to 20, while when downloading from a slow network with a
    // fast multicore CPU, it won't be much higher than 1.
    static const double fSigcheckVerificationFactor = 5.0;

    struct CCheckpointData {
        const MapCheckpoints *mapCheckpoints;
        int64 nTimeLastCheckpoint;
        int64 nTransactionsLastCheckpoint;
        double fTransactionsPerDay;
    };

    // What makes a good checkpoint block?
    // + Is surrounded by blocks with reasonable timestamps
    //   (no blocks before with a timestamp after, none after with
    //    timestamp before)
    // + Contains no strange transactions
    static MapCheckpoints mapCheckpoints =
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
                ;
    static const CCheckpointData data = {
        &mapCheckpoints,
        1406762185, // * UNIX timestamp of last checkpoint block
        185337,    // * total number of transactions between genesis and last checkpoint
                    //   (the tx=... number in the SetBestChain debug.log lines)
        12000.0     // * estimated number of transactions per day after checkpoint
    };

    static MapCheckpoints mapCheckpointsTestnet =
        boost::assign::map_list_of
        (   546, uint256("0xa0fea99a6897f531600c8ae53367b126824fd6a847b2b2b73817a95b8e27e602"))
        ;
    static const CCheckpointData dataTestnet = {
        &mapCheckpointsTestnet,
        1406762185
    };

    const CCheckpointData &Checkpoints() {
        if (fTestNet)
            return dataTestnet;
        else
            return data;
    }

    bool CheckBlock(int nHeight, const uint256& hash)
    {
        if (!GetBoolArg("-checkpoints", true))
            return true;

        const MapCheckpoints& checkpoints = *Checkpoints().mapCheckpoints;

        MapCheckpoints::const_iterator i = checkpoints.find(nHeight);
        if (i == checkpoints.end()) return true;
        return hash == i->second;
    }

    // Guess how far we are in the verification process at the given block index
    double GuessVerificationProgress(CBlockIndex *pindex) {
        if (pindex==NULL)
            return 0.0;

        int64 nNow = time(NULL);

        double fWorkBefore = 0.0; // Amount of work done before pindex
        double fWorkAfter = 0.0;  // Amount of work left after pindex (estimated)
        // Work is defined as: 1.0 per transaction before the last checkoint, and
        // fSigcheckVerificationFactor per transaction after.

        const CCheckpointData &data = Checkpoints();

        if (pindex->nChainTx <= data.nTransactionsLastCheckpoint) {
            double nCheapBefore = pindex->nChainTx;
            double nCheapAfter = data.nTransactionsLastCheckpoint - pindex->nChainTx;
            double nExpensiveAfter = (nNow - data.nTimeLastCheckpoint)/86400.0*data.fTransactionsPerDay;
            fWorkBefore = nCheapBefore;
            fWorkAfter = nCheapAfter + nExpensiveAfter*fSigcheckVerificationFactor;
        } else {
            double nCheapBefore = data.nTransactionsLastCheckpoint;
            double nExpensiveBefore = pindex->nChainTx - data.nTransactionsLastCheckpoint;
            double nExpensiveAfter = (nNow - pindex->nTime)/86400.0*data.fTransactionsPerDay;
            fWorkBefore = nCheapBefore + nExpensiveBefore*fSigcheckVerificationFactor;
            fWorkAfter = nExpensiveAfter*fSigcheckVerificationFactor;
        }

        return fWorkBefore / (fWorkBefore + fWorkAfter);
    }

    int GetTotalBlocksEstimate()
    {
        if (!GetBoolArg("-checkpoints", true))
            return 0;

        const MapCheckpoints& checkpoints = *Checkpoints().mapCheckpoints;

        return checkpoints.rbegin()->first;
    }

    CBlockIndex* GetLastCheckpoint(const std::map<uint256, CBlockIndex*>& mapBlockIndex)
    {
        if (!GetBoolArg("-checkpoints", true))
            return NULL;

        const MapCheckpoints& checkpoints = *Checkpoints().mapCheckpoints;

        BOOST_REVERSE_FOREACH(const MapCheckpoints::value_type& i, checkpoints)
        {
            const uint256& hash = i.second;
            std::map<uint256, CBlockIndex*>::const_iterator t = mapBlockIndex.find(hash);
            if (t != mapBlockIndex.end())
                return t->second;
        }
        return NULL;
    }
}
