// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>
#include <auxpow.h>
#include <arith_uint256.h>
#include <chain.h>
#include <primitives/block.h>
#include <uint256.h>
#include <util.h>

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    int DiffMode = 1;
        // if(! params.NetworkIDString() == "main" ){
        //         if (pindexLast->nHeight+1 >= 15) { DiffMode = 1; }
        //         else if (pindexLast->nHeight+1 >= 5) { DiffMode = 1; }
        // }
        // else {
                if (pindexLast->nHeight+1 >= 120000) { DiffMode = 4; }
                if (pindexLast->nHeight+1 >= 126000) { DiffMode = 5; }

        //}
        if (DiffMode == 1) { return GetNextWorkRequired_V1(pindexLast, pblock, params); }
        else if (DiffMode == 4) { return DarkGravityWave2(pindexLast, pblock, params); }
        else if (DiffMode == 5) { return DarkGravityWave3(pindexLast, pblock, params); }
    return DarkGravityWave3(pindexLast, pblock, params);
}

unsigned int GetNextWorkRequired_V1(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Genesis block
    if (pindexLast == NULL)
        return nProofOfWorkLimit;

    int nHeight = pindexLast->nHeight + 1;
    int64_t nTargetSpacing = params.nPowTargetSpacing;
    int64_t nTargetTimespan = params.nPowTargetTimespan;
    int64_t nInterval = params.DifficultyAdjustmentInterval();

    // Marscoin: 1 sol (every Mars sol retarget)
    int nForkOne = 14260;
    int nForkTwo = 70000;
    if(nHeight >= nForkOne)
    {
      //printf("Retargeting to sol day");
      nTargetTimespan = 88775; //Marscoin: 1 Mars-day has 88775 seconds
    }
    if(nHeight >= nForkTwo)
    {
      //printf("Retargeting to sol day");
      nTargetTimespan = 88775;
      nTargetSpacing = 123; //Marscoin: 2 Mars-minutes. 1 Mars-second is 61.649486615 seconds
      nInterval = nTargetTimespan / nTargetSpacing; 
    }


    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight+1) % nInterval != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + nTargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % nInterval != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    // Marscoin: This fixes an issue where a 51% attack can change difficulty at will.
    // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
    int blockstogoback = nInterval-1;
    if ((pindexLast->nHeight+1) != nInterval)
        blockstogoback = nInterval;

    // Go back by what we want to be 14 days worth of blocks
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < blockstogoback; i++)
        pindexFirst = pindexFirst->pprev;

    assert(pindexFirst);
    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);

}


unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    //--
    int nHeight = pindexLast->nHeight + 1;
    int64_t nTargetSpacing = params.nPowTargetSpacing;
    int64_t nTargetTimespan = params.nPowTargetTimespan;
    int64_t nInterval = params.DifficultyAdjustmentInterval();

    // Marscoin: 1 sol (every Mars sol retarget)
    int nForkOne = 14260;
    int nForkTwo = 70000;
    if(nHeight >= nForkOne)
    {
      //printf("Retargeting to sol day");
      nTargetTimespan = 88775; //Marscoin: 1 Mars-day has 88775 seconds
    }
    if(nHeight >= nForkTwo)
    {
      //printf("Retargeting to sol day");
      nTargetTimespan = 88775;
      nTargetSpacing = 123; //Marscoin: 2 Mars-minutes. 1 Mars-second is 61.649486615 seconds
      nInterval = nTargetTimespan / nTargetSpacing; 
    }
    //----

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < nTargetTimespan/4)
        nActualTimespan = nTargetTimespan/4;
    if (nActualTimespan > nTargetTimespan*4)
        nActualTimespan = nTargetTimespan*4;

    // Retarget
    arith_uint256 bnNew;
    arith_uint256 bnOld;
    bnNew.SetCompact(pindexLast->nBits);
    bnOld = bnNew;
    // Marscoin: intermediate uint256 can overflow by 1 bit
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    bool fShift = bnNew.bits() > bnPowLimit.bits() - 1;
    if (fShift)
        bnNew >>= 1;
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;
    if (fShift)
        bnNew <<= 1;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    /// debug print
//    LogPrintf("GetNextWorkRequired RETARGET\n");
//    LogPrintf("Params().TargetTimespan() = %d    nActualTimespan = %d\n", nTargetTimespan, nActualTimespan);
//    LogPrintf("Before: %08x  %s\n", pindexLast->nBits, bnOld.ToString());
//    LogPrintf("After:  %08x  %s\n", bnNew.GetCompact(), bnNew.ToString());

    return bnNew.GetCompact();
}


bool CheckAuxPowProofOfWork(const CBlockHeader& block, const Consensus::Params& params)
{
    /* Except for legacy blocks with full version 4, ensure that
       the chain ID is correct.  Legacy blocks are not allowed since
       the merge-mining start, which is checked in AcceptBlockHeader
       where the height is known.  */
    if (!block.IsLegacy() && params.fStrictChainId && block.GetChainId() != params.nAuxpowChainId)
        return error("%s : block does not have our chain ID"
                     " (got %d, expected %d, full nVersion %d)",
                     __func__, block.GetChainId(),
                     params.nAuxpowChainId, block.nVersion);

    /* If there is no auxpow, just check the block hash.  */
    if (!block.auxpow)
    {
        if (block.IsAuxpow())
            return error("%s : no auxpow on block with auxpow version",
                         __func__);

        if (!CheckProofOfWork(block.GetPoWHash(), block.nBits, params))
            return error("%s : non-AUX proof of work failed", __func__);

        return true;
    }

    /* We have auxpow.  Check it.  */

    if (!block.IsAuxpow())
        return error("%s : auxpow on block with non-auxpow version", __func__);

    if (!block.auxpow->check(block.GetHash(), block.GetChainId(), params))
        return error("%s : AUX POW is not valid", __func__);
    if (!CheckProofOfWork(block.auxpow->getParentBlockPoWHash(), block.nBits, params))
        return error("%s : AUX proof of work failed", __func__);

    return true;
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}





//original
unsigned int DarkGravityWave2(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    /* current difficulty formula, darkcoin / dash - DarkGravity v2, written by Evan Duffield -  */
    const CBlockIndex *BlockLastSolved = pindexLast;
    const CBlockIndex *BlockReading = pindexLast;
    const CBlockHeader *BlockCreating = pblock;
    BlockCreating = BlockCreating;
    int64_t nBlockTimeAverage = 0;
    int64_t nBlockTimeAveragePrev = 0;
    int64_t nBlockTimeCount = 0;
    int64_t nBlockTimeSum2 = 0;
    int64_t nBlockTimeCount2 = 0;
    int64_t LastBlockTime = 0;
    int64_t PastBlocksMin = 14;
    int64_t PastBlocksMax = 140;
    int64_t CountBlocks = 0;
    arith_uint256 PastDifficultyAverage;
    arith_uint256 PastDifficultyAveragePrev;
    //LogPrintf("GravityWave2===================================\n");
    int64_t nTargetSpacing = 123; //Marscoin: 2 Mars-minutes. 1 Mars-second is 61.649486615 seconds

    //LogPrintf("powLimit: %i\n", UintToArith256(params.powLimit).GetCompact());
    if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0 || BlockLastSolved->nHeight < PastBlocksMin) { return UintToArith256(params.powLimit).GetCompact(); }

    //LogPrintf("GravityWave2 setup complete\n");
    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0; i++) 
    {
        if (PastBlocksMax > 0 && i > PastBlocksMax) { break; }
        CountBlocks++;

        if(CountBlocks <= PastBlocksMin) 
        {
            if (CountBlocks == 1) { 
                PastDifficultyAverage.SetCompact(BlockReading->nBits); 
                //LogPrintf("Count1: PastDifficultyAvg: %i\n", arith_uint256(PastDifficultyAverage).GetCompact());
            }
            
            else 
            { 
                //PastDifficultyAverage = ( ( arith_uint256().SetCompact(BlockReading->nBits) - PastDifficultyAveragePrev) / CountBlocks) + PastDifficultyAveragePrev; 
                PastDifficultyAverage = ((PastDifficultyAveragePrev * CountBlocks) + (arith_uint256().SetCompact(BlockReading->nBits))) / (CountBlocks + 1);
//                LogPrintf("Part0: %i\n", BlockReading->nBits);
//                LogPrintf("Part1: %i\n", arith_uint256().SetCompact(BlockReading->nBits).GetCompact());
//                LogPrintf("Part2 (minus): %i\n", PastDifficultyAveragePrev.GetCompact());
//                LogPrintf("Part3 (minus): %i\n", CountBlocks);
//                LogPrintf("Part4 (minus): %i\n", PastDifficultyAveragePrev.GetCompact());
//                LogPrintf("Sum Part 1: %i\n", arith_uint256( arith_uint256().SetCompact(BlockReading->nBits) - PastDifficultyAveragePrev).GetCompact()  );
//                LogPrintf("Sum Part 2: %i\n", PastDifficultyAveragePrev.GetCompact());
//                LogPrintf("Else: PastDifficultyAvg: %i\n", arith_uint256(PastDifficultyAverage).GetCompact());
//                LogPrintf("Else: PastDifficultyAvgPrev: %i\n", arith_uint256(PastDifficultyAveragePrev).GetCompact());
            }
            PastDifficultyAveragePrev = PastDifficultyAverage;
        }
        //LogPrintf("PastDifficultyAvg: %i\n", arith_uint256(PastDifficultyAveragePrev).GetCompact());

        if(LastBlockTime > 0){
            int64_t Diff = (LastBlockTime - BlockReading->GetBlockTime());
            if(nBlockTimeCount <= PastBlocksMin) {
                nBlockTimeCount++;
                if (nBlockTimeCount == 1) { nBlockTimeAverage = Diff; }
                else { nBlockTimeAverage = ((Diff - nBlockTimeAveragePrev) / nBlockTimeCount) + nBlockTimeAveragePrev; }
                nBlockTimeAveragePrev = nBlockTimeAverage;
            }
            nBlockTimeCount2++;
            nBlockTimeSum2 += Diff;
            //LogPrintf("PastDifficultyAvg: %i\n", arith_uint256(PastDifficultyAveragePrev).GetCompact());
        }
        LastBlockTime = BlockReading->GetBlockTime();

        if (BlockReading->pprev == NULL) { assert(BlockReading); break; }
        BlockReading = BlockReading->pprev;
    }
    //LogPrintf("BlocktimeAvg: %i \n", nBlockTimeAveragePrev);

    arith_uint256 bnNew(PastDifficultyAverage);
    //LogPrintf("bnNew Setup: %i \n", bnNew.GetCompact());
    if (nBlockTimeCount != 0 && nBlockTimeCount2 != 0) 
    {
//            LogPrintf("nBlockTimeAverage: %d \n", nBlockTimeAverage);
//            LogPrintf("nBlockTimeSum2: %d \n", nBlockTimeSum2);
//            LogPrintf("nBlockTimeCount2: %d \n", nBlockTimeCount2);
            
            double SmartAverage = ((((long double)nBlockTimeAverage)*0.7)+(((long double)nBlockTimeSum2 / (long double)nBlockTimeCount2)*0.3));
            //LogPrintf("SmartAverage: %d \n", SmartAverage);
            
            if(SmartAverage < 1) SmartAverage = 1;
            
            //LogPrintf("TargetSpacing1: %d \n", nTargetSpacing);
            
            double Shift = nTargetSpacing/SmartAverage;
            //LogPrintf("Shift: %d \n", Shift);
            
            double fActualTimespan = ((long double)CountBlocks*(double)nTargetSpacing)/Shift;
            double fTargetTimespan = ((long double)CountBlocks*(double)nTargetSpacing);
            
            //LogPrintf("fActualTimespan: %d \n", fActualTimespan);
            //LogPrintf("fTargetTimespan: %d \n", fTargetTimespan);
            
            if (fActualTimespan < fTargetTimespan/3){
                fActualTimespan = fTargetTimespan/3;
                //LogPrintf("I guess it's this time of the year");
            }
            if (fActualTimespan > fTargetTimespan*3)
                fActualTimespan = fTargetTimespan*3;

            //LogPrintf("bnNew before application: %i \n", bnNew.GetCompact());
            int64_t nActualTimespan = fActualTimespan;
            int64_t nTargetTimespan = fTargetTimespan;
            //LogPrintf("nActualTimespan: %d \n", nActualTimespan);
            //LogPrintf("nTargetTimespan: %d \n", nTargetTimespan);

            // Retarget
            bnNew *= nActualTimespan;
            //LogPrintf("Retarget1: %i using nActualTimespan %i \n", bnNew.GetCompact(), nActualTimespan);
            bnNew /= nTargetTimespan;
            //LogPrintf("Retarget2: %i using nTargetTimespan %i \n", bnNew.GetCompact(), nTargetTimespan);
    }
    //LogPrintf("GravityWave2 calculation complete\n");
    //LogPrintf("bnNew: %i > %i \n", bnNew.GetCompact(), UintToArith256(params.powLimit).GetCompact());

    if (bnNew.GetCompact() > UintToArith256(params.powLimit).GetCompact()){
        bnNew.SetCompact(UintToArith256(params.powLimit).GetCompact());
        //LogPrintf("PowLimit: %i\n", bnNew.GetCompact());
    }

    return bnNew.GetCompact();
}


unsigned int DarkGravityWave3(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    /* current difficulty formula, darkcoin - DarkGravity v3, written by Evan Duffield - evan@darkcoin.io */
    const CBlockIndex *BlockLastSolved = pindexLast;
    const CBlockIndex *BlockReading = pindexLast;
    const CBlockHeader *BlockCreating = pblock;
    BlockCreating = BlockCreating;
    int64_t nActualTimespan = 0;
    int64_t LastBlockTime = 0;
    int64_t PastBlocksMin = 24;
    int64_t PastBlocksMax = 24;
    int64_t CountBlocks = 0;
    arith_uint256 PastDifficultyAverage;
    arith_uint256 PastDifficultyAveragePrev;
    //LogPrintf("GravityWave3===================================\n");
    int64_t nTargetSpacing = params.nPowTargetSpacing;
    int64_t nTargetTimespan = 88775; //Marscoin: 1 Mars-day has 88775 seconds
    nTargetSpacing = 123; //Marscoin: 2 Mars-minutes. 1 Mars-second is 61.649486615 seconds

    if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0 || BlockLastSolved->nHeight < PastBlocksMin) {
        return UintToArith256(params.powLimit).GetCompact();
    }

    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0; i++) {
        if (PastBlocksMax > 0 && i > PastBlocksMax) { break; }
        CountBlocks++;

        if(CountBlocks <= PastBlocksMin) {
            if (CountBlocks == 1) { PastDifficultyAverage.SetCompact(BlockReading->nBits); }
            else { PastDifficultyAverage = ((PastDifficultyAveragePrev * CountBlocks)+(arith_uint256().SetCompact(BlockReading->nBits))) / (CountBlocks+1); }
            PastDifficultyAveragePrev = PastDifficultyAverage;
        }

        if(LastBlockTime > 0){
            int64_t Diff = (LastBlockTime - BlockReading->GetBlockTime());
            nActualTimespan += Diff;
        }
        LastBlockTime = BlockReading->GetBlockTime();

        if (BlockReading->pprev == NULL) { assert(BlockReading); break; }
        BlockReading = BlockReading->pprev;
    }

    arith_uint256 bnNew(PastDifficultyAverage);

    nTargetTimespan = CountBlocks*nTargetSpacing;

    if (nActualTimespan < nTargetTimespan/3)
        nActualTimespan = nTargetTimespan/3;
    if (nActualTimespan > nTargetTimespan*3)
        nActualTimespan = nTargetTimespan*3;

    // Retarget
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;

    if (bnNew.GetCompact() > UintToArith256(params.powLimit).GetCompact()){
        bnNew.SetCompact(UintToArith256(params.powLimit).GetCompact());
    }

    return bnNew.GetCompact();
}
