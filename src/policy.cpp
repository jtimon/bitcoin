// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// NOTE: This file is intended to be customised by the end user, and includes only local node policy logic

#include "policy.h"

#include "amount.h"
#include "main.h"
#include "primitives/transaction.h"
#include "sync.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "uint256.h"
#include "util.h"
#include "utilmoneystr.h"

#include <cmath>
#include <string>

/** Fees smaller than this (in satoshi) are considered zero fee (for relaying and mining) */
CFeeRate minRelayTxFee = CFeeRate(1000);

CAmount CNodePolicy::GetMinRelayFee(const CTransaction& tx, unsigned int nBytes, bool fAllowFree)
{
    {
        LOCK(mempool.cs);
        uint256 hash = tx.GetHash();
        double dPriorityDelta = 0;
        CAmount nFeeDelta = 0;
        mempool.ApplyDeltas(hash, dPriorityDelta, nFeeDelta);
        if (dPriorityDelta > 0 || nFeeDelta > 0)
            return 0;
    }

    CAmount nMinFee = minRelayTxFee.GetFee(nBytes);

    if (fAllowFree)
    {
        // There is a free transaction area in blocks created by most miners,
        // * If we are relaying we allow transactions up to DEFAULT_BLOCK_PRIORITY_SIZE - 1000
        //   to be considered to fall into this category. We don't want to encourage sending
        //   multiple transactions instead of one big transaction to avoid fees.
        if (nBytes < (DEFAULT_BLOCK_PRIORITY_SIZE - 1000))
            nMinFee = 0;
    }

    if (!MoneyRange(nMinFee))
        nMinFee = MAX_MONEY;
    return nMinFee;
}

bool CNodePolicy::AcceptTxPoolPreInputs(CTxMemPool& pool, CValidationState& state, const CTransaction& tx)
{
    // Rather not work on nonstandard transactions (unless -testnet/-regtest)
    std::string reason;
    if (fRequireStandardTx && !IsStandardTx(tx, reason))
        return state.DoS(0,
                         error("%s : nonstandard transaction: %s", __func__, reason),
                         REJECT_NONSTANDARD, reason);

    // Check for conflicts with in-memory transactions
    if (pool.lookupConflicts(tx, NULL))
    {
        // Disable replacement feature for now
        return false;
    }

    return true;
}

bool CNodePolicy::AcceptTxWithInputs(CTxMemPool& pool, CValidationState& state, const CTransaction& tx, CCoinsViewCache& view)
{
    // Check for non-standard pay-to-script-hash in inputs
    if (fRequireStandardTx && !AreInputsStandard(tx, view))
        return error("%s : nonstandard transaction input", __func__);

    // Check that the transaction doesn't have an excessive number of
    // sigops, making it impossible to mine. Since the coinbase transaction
    // itself can contain sigops MAX_TX_SIGOPS is less than
    // MAX_BLOCK_SIGOPS; we still consider this an invalid rather than
    // merely non-standard transaction.
    unsigned int nSigOps = GetLegacySigOpCount(tx);
    nSigOps += GetP2SHSigOpCount(tx, view);
    if (nSigOps > MAX_TX_SIGOPS)
        return state.DoS(0,
                         error("%s : too many sigops %s, %d > %d",
                               __func__, tx.GetHash().ToString(), nSigOps, MAX_TX_SIGOPS),
                         REJECT_NONSTANDARD, "bad-txns-too-many-sigops");

    return true;
}

bool CNodePolicy::AcceptMemPoolEntry(CTxMemPool& pool, CValidationState& state, CTxMemPoolEntry& entry, CCoinsViewCache& view, bool& fRateLimit)
{
    const CTransaction& tx = entry.GetTx();

    CAmount nFees = entry.GetFee();
    unsigned int nSize = entry.GetTxSize();

    // Don't accept it if it can't get into a block
    CAmount txMinFee = GetMinRelayFee(tx, nSize, true);
    if (nFees < txMinFee)
        return state.DoS(0, error("%s : not enough fees %s, %d < %d",
                                  __func__, tx.GetHash().ToString(), nFees, txMinFee),
                         REJECT_INSUFFICIENTFEE, "insufficient fee");

    // Continuously rate-limit free (really, very-low-fee)transactions
    // This mitigates 'penny-flooding' -- sending thousands of free transactions just to
    // be annoying or make others' transactions take longer to confirm.
    fRateLimit = (nFees < ::minRelayTxFee.GetFee(nSize));

    return true;
}

bool CNodePolicy::RateLimitTx(CTxMemPool& pool, CValidationState& state, CTxMemPoolEntry& entry, CCoinsViewCache& view)
{
    static CCriticalSection csFreeLimiter;
    static double dFreeCount;
    static int64_t nLastTime;
    int64_t nNow = GetTime();

    LOCK(csFreeLimiter);

    // Use an exponentially decaying ~10-minute window:
    dFreeCount *= pow(1.0 - 1.0/600.0, (double)(nNow - nLastTime));
    nLastTime = nNow;
    // -limitfreerelay unit is thousand-bytes-per-minute
    // At default rate it would take over a month to fill 1GB
    if (dFreeCount >= GetArg("-limitfreerelay", 15)*10*1000)
        return state.DoS(0, error("%s : free transaction rejected by rate limiter", __func__),
                         REJECT_INSUFFICIENTFEE, "insufficient priority");
    unsigned int nSize = entry.GetTxSize();
    LogPrint("mempool", "Rate limit dFreeCount: %g => %g\n", dFreeCount, dFreeCount+nSize);
    dFreeCount += nSize;

    return true;
}

CNodePolicy policy;

void InitPolicyFromCommandLine()
{
    // Fee-per-kilobyte amount considered the same as "free"
    // If you are mining, be careful setting this:
    // if you set it to zero then
    // a transaction spammer can cheaply fill blocks using
    // 1-satoshi-fee transactions. It should be set above the real
    // cost to you of processing a transaction.
    if (mapArgs.count("-minrelaytxfee"))
    {
        CAmount n = 0;
        if (ParseMoney(mapArgs["-minrelaytxfee"], n) && n > 0)
            ::minRelayTxFee = CFeeRate(n);
        else
            throw std::runtime_error(strprintf(_("Invalid amount for -minrelaytxfee=<amount>: '%s'"), mapArgs["-minrelaytxfee"]));
    }
    policy.fRequireStandardTx = Params().RequireStandard();
}
