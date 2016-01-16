// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_STORAGE_INTERFACES_CPP_H
#define BITCOIN_CONSENSUS_STORAGE_INTERFACES_CPP_H

#include "uint256.h"

#include <algorithm>

class CTransaction;
class CTxOut;

// CPP storage interfaces

class CBlockIndexView
{
public:
    CBlockIndexView() {};
    virtual ~CBlockIndexView() {};

    virtual const uint256 GetBlockHash() const = 0;
    //! Efficiently find an ancestor of this block.
    virtual const CBlockIndexView* GetAncestorView(int64_t height) const = 0;
    virtual int64_t GetHeight() const = 0;
    virtual int32_t GetVersion() const = 0;
    virtual int32_t GetTime() const = 0;
    virtual int32_t GetBits() const = 0;
    // Potential optimizations
    virtual const CBlockIndexView* GetPrev() const
    {
        return GetAncestorView(GetHeight() - 1);
    };
    virtual int64_t GetMedianTimePast() const
    {
        unsigned nMedianTimeSpan = 11; // Consensus::Params? Not a priority
        int64_t pmedian[nMedianTimeSpan];
        int64_t* pbegin = &pmedian[nMedianTimeSpan];
        int64_t* pend = &pmedian[nMedianTimeSpan];

        const CBlockIndexView* pindex = this;
        for (unsigned i = 0; i < nMedianTimeSpan && pindex; i++, pindex = pindex->GetPrev())
            *(--pbegin) = pindex->GetTime();

        std::sort(pbegin, pend);
        return pbegin[(pend - pbegin)/2];
    }
};

class CCoinsInterface
{
public:
    CCoinsInterface() {};
    virtual ~CCoinsInterface() {};    

    //! check whether a particular output is still available or spent
    virtual bool IsAvailable(int32_t nPos) const = 0;
    virtual bool IsCoinBase() const = 0;
    //! check whether the entire CCoins is spent
    //! note that only !IsPruned() CCoins can be serialized
    virtual bool IsPruned() const = 0;
    virtual const CTxOut& GetOutput(int32_t nPos) const = 0;
    virtual int64_t GetHeight() const = 0;
};

class CUtxoView
{
public:
    CUtxoView() {};
    virtual ~CUtxoView() {};

    //! Check whether all prevouts of the transaction are present in the UTXO set represented by this view
    virtual bool HaveInputs(const CTransaction& tx) const = 0;
    /**
     * Return a pointer to CCoins in the cache, or NULL if not found. This is
     * more efficient than GetCoins. Modifications to other cache entries are
     * allowed while accessing the returned pointer.
     */
    virtual const CCoinsInterface* AccessCoins(const uint256 &txid) const = 0;
    virtual const CTxOut& GetOutput(const uint256& txid, int32_t nPos) const
    {
        const CCoinsInterface* coins = AccessCoins(txid);
        assert(coins && coins->IsAvailable(nPos));
        return coins->GetOutput(nPos);
    };

};

#endif // BITCOIN_CONSENSUS_STORAGE_INTERFACES_CPP_H
