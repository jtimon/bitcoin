// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"

#include "arith_uint256.h"
#include "chain.h"
#include "consensus/validation.h"
#include "primitives/block.h"
#include "uint256.h"

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Genesis block
    if (pindexLast == NULL)
        return nProofOfWorkLimit;

    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight+1) % params.DifficultyAdjustmentInterval() != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval()-1);
    assert(nHeightFirst >= 0);
    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

/** Check whether a block hash satisfies the proof-of-work requirement specified by nBits */
static bool CheckProofOfWork(const Consensus::Params& params, uint256 hash, unsigned int nBits, CValidationState& state)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return state.DoS(50, false, REJECT_INVALID, "high-hash-range", false, "proof of work failed");

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return state.DoS(50, false, REJECT_INVALID, "high-hash-target", false, "proof of work failed");

    return true;
}

bool CheckProof(const Consensus::Params& params, const uint256& blockHash, const CBlockHeader& block, CValidationState& state)
{
    return CheckProofOfWork(params, blockHash, block.nBits, state);
}

bool MaybeGenerateProof(const Consensus::Params& params, CBlockHeader* pblock, uint64_t& nTries)
{
    CValidationState state;
    static const int nInnerLoopCount = 0x10000;
    uint256 blockHash = pblock->GetHash();
    while (nTries > 0 && pblock->nNonce < nInnerLoopCount && !CheckProofOfWork(params, blockHash, pblock->nBits, state)) {
        ++pblock->nNonce;
        blockHash = pblock->GetHash();
        --nTries;
    }
    return CheckProof(params, blockHash, *pblock, state);
}

bool GenerateProof(const Consensus::Params& params, CBlockHeader* pblock)
{
    uint64_t nTries = 10000;
    return MaybeGenerateProof(params, pblock, nTries);
}

void ResetProof(CBlockHeader* pblock)
{
    pblock->nNonce = 0;
}
