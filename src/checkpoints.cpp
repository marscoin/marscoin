// Copyright (c) 2009-2012 The Marscoin developers
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
        boost::assign::map_list_of //Checkpoint 0 hash == Genesis block hash.
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
                (    121000, uint256("0x6cbf3996f30e47e6cfb8ba05c9c9aba5576bf0b12d88562f8a233c978f80a152"))
                (    121994, uint256("0xf1fc870f45046aafd0e8c8e7f4a74e40d02e84d79857049461efc3be28d40098"))
        ;
    static const CCheckpointData data = {
        &mapCheckpoints,
        1390102573, // * UNIX timestamp of last checkpoint block
        16216,      // * total number of transactions between genesis and last checkpoint
                    //   (the tx=... number in the SetBestChain debug.log lines)
        1400.0     // * estimated number of transactions per day after checkpoint
    };

    static MapCheckpoints mapCheckpointsTestnet = 
        boost::assign::map_list_of
                    (         0, uint256("0x06e005f86644f15d2e4c62b59a038c798a3b0816ba58dcc8c91e02ce5a685299"))
        ;
    static const CCheckpointData dataTestnet = {
        &mapCheckpointsTestnet,
        1388590627,
        0,
        300
    };

    const CCheckpointData &Checkpoints() {
        if (fTestNet)
            return dataTestnet;
        else
            return data;
    }

    bool CheckBlock(int nHeight, const uint256& hash)
    {
        if (fTestNet) return true; // Testnet has no checkpoints
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
        if (fTestNet) return 0; // Testnet has no checkpoints
        if (!GetBoolArg("-checkpoints", true))
            return 0;

        const MapCheckpoints& checkpoints = *Checkpoints().mapCheckpoints;

        return checkpoints.rbegin()->first;
    }

    CBlockIndex* GetLastCheckpoint(const std::map<uint256, CBlockIndex*>& mapBlockIndex)
    {
        if (fTestNet) return NULL; // Testnet has no checkpoints
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
