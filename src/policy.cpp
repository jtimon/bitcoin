// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// NOTE: This file is intended to be customised by the end user, and includes only local node policy logic

#include "policy.h"

#include "chainparams.h"
#include "coins.h"
#include "main.h"
#include "primitives/transaction.h"
#include "ui_interface.h"
#include "uint256.h"
#include "util.h"
#include "utilmoneystr.h"

#include <cmath>

#include <boost/foreach.hpp>

static bool fIsBareMultisigStd = true;
/** Fees smaller than this (in satoshi) are considered zero fee (for relaying and mining) */
CFeeRate minRelayTxFee = CFeeRate(1000);
static unsigned nMaxDatacarrierBytes = MAX_OP_RETURN_RELAY;

/** Standard Policy implementing CPolicy */
class CStandardPolicy : public CPolicy
{
public:
    virtual bool CheckScript(const CScript& scriptPubKey, txnouttype& whichType) const;
    virtual bool CheckOutput(const CTxOut& txout) const;
    virtual bool CheckTxPreInputs(const CTransaction& tx, std::string& reason) const;
    /**
     * Check transaction inputs to mitigate two
     * potential denial-of-service attacks:
     * 
     * 1. scriptSigs with extra data stuffed into them,
     *    not consumed by scriptPubKey (or P2SH script)
     * 2. P2SH scripts with a crazy number of expensive
     *    CHECKSIG/CHECKMULTISIG operations
     */
    virtual bool CheckTxWithInputs(const CTransaction& tx, const CCoinsViewCache& mapInputs) const;
    virtual CAmount GetMinRelayFee(const CTransaction&, unsigned int nBytes, bool fAllowFree) const;
    virtual bool AcceptTxPoolPreInputs(CTxMemPool&, CValidationState&, const CTransaction&) const;
    virtual bool AcceptTxWithInputs(CTxMemPool&, CValidationState&, const CTransaction&, CCoinsViewCache&) const;
    virtual bool AcceptMemPoolEntry(CTxMemPool&, CValidationState&, CTxMemPoolEntry&, CCoinsViewCache&, bool& fRateLimit) const;
    virtual bool RateLimitTx(CTxMemPool&, CValidationState&, CTxMemPoolEntry&, CCoinsViewCache&) const;
};

/** Default Policy for testnet and regtest */
class CTestPolicy : public CStandardPolicy 
{
public:
    virtual bool CheckTxPreInputs(const CTransaction& tx, std::string& reason) const
    {
        return true;
    }
    virtual bool CheckTxWithInputs(const CTransaction& tx, const CCoinsViewCache& mapInputs) const
    {
        return true;
    }
};

static CStandardPolicy standardPolicy;
static CTestPolicy testPolicy;

static CPolicy* pCurrentPolicy = 0;

CPolicy& Policy(std::string policy)
{
    if (policy == "standard")
        return standardPolicy;
    else if (policy == "test")
        return testPolicy;
    throw std::runtime_error("Unknown policy " + policy + "\n");
}

void SelectPolicy(std::string policyType)
{
    pCurrentPolicy = &Policy(policyType);
}

const CPolicy& Policy()
{
    assert(pCurrentPolicy);
    return *pCurrentPolicy;
}

void InitPolicyFromCommandLine()
{
    std::string policyArg = GetArg("-policy", Params().DefaultPolicy());
    SelectPolicy(policyArg);
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
    fIsBareMultisigStd = GetArg("-permitbaremultisig", true) != 0;
    if (GetBoolArg("-datacarrier", true))
        nMaxDatacarrierBytes = GetArg("-datacarriersize", MAX_OP_RETURN_RELAY);
    else
        nMaxDatacarrierBytes = 0;
}

// CStandardPolicy implementation
bool CStandardPolicy::CheckScript(const CScript& scriptPubKey, txnouttype& whichType) const
{
    std::vector<std::vector<unsigned char> > vSolutions;
    if (!Solver(scriptPubKey, whichType, vSolutions))
        return false;

    switch (whichType)
    {
        case TX_MULTISIG:
        {
            unsigned char m = vSolutions.front()[0];
            unsigned char n = vSolutions.back()[0];
            // Support up to x-of-3 multisig txns as standard
            if (n < 1 || n > 3)
                return false;
            if (m < 1 || m > n)
                return false;
            break;
        }

        case TX_NULL_DATA:
            // TX_NULL_DATA without any vSolutions is a lone OP_RETURN, which traditionally is accepted regardless of the -datacarrier option, so we skip the check.
            // If you want to filter lone OP_RETURNs, be sure to handle vSolutions being empty below where vSolutions.front() is accessed!
            if (vSolutions.size())
            {
                if (!nMaxDatacarrierBytes)
                    return false;

                if (vSolutions.front().size() > nMaxDatacarrierBytes)
                    return false;
            }

            break;

        default:
            // no other restrictions on standard scripts
            break;
    }

    return whichType != TX_NONSTANDARD;
}

bool CStandardPolicy::CheckOutput(const CTxOut& txout) const
{
    // "Dust" is defined in terms of CTransaction::minRelayTxFee,
    // which has units satoshis-per-kilobyte.
    // If you'd pay more than 1/3 in fees
    // to spend something, then we consider it dust.
    // A typical txout is 34 bytes big, and will
    // need a CTxIn of at least 148 bytes to spend:
    // so dust is a txout less than 546 satoshis 
    // with default minRelayTxFee.
    size_t nSize = txout.GetSerializeSize(SER_DISK,0) + 148u;
    return (txout.nValue < 3 * minRelayTxFee.GetFee(nSize));
}

bool CStandardPolicy::CheckTxPreInputs(const CTransaction& tx, std::string& reason) const
{
    if (tx.nVersion > CTransaction::CURRENT_VERSION || tx.nVersion < 1) {
        reason = "version";
        return false;
    }

    // Extremely large transactions with lots of inputs can cost the network
    // almost as much to process as they cost the sender in fees, because
    // computing signature hashes is O(ninputs*txsize). Limiting transactions
    // to MAX_STANDARD_TX_SIZE mitigates CPU exhaustion attacks.
    unsigned int sz = tx.GetSerializeSize(SER_NETWORK, CTransaction::CURRENT_VERSION);
    if (sz >= MAX_STANDARD_TX_SIZE) {
        reason = "tx-size";
        return false;
    }

    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        // Biggest 'standard' txin is a 15-of-15 P2SH multisig with compressed
        // keys. (remember the 520 byte limit on redeemScript size) That works
        // out to a (15*(33+1))+3=513 byte redeemScript, 513+1+15*(73+1)+3=1627
        // bytes of scriptSig, which we round off to 1650 bytes for some minor
        // future-proofing. That's also enough to spend a 20-of-20
        // CHECKMULTISIG scriptPubKey, though such a scriptPubKey is not
        // considered standard)
        if (txin.scriptSig.size() > 1650) {
            reason = "scriptsig-size";
            return false;
        }
        if (!txin.scriptSig.IsPushOnly()) {
            reason = "scriptsig-not-pushonly";
            return false;
        }
    }

    unsigned int nDataOut = 0;
    txnouttype whichType;
    BOOST_FOREACH(const CTxOut& txout, tx.vout) {
        if (!CheckScript(txout.scriptPubKey, whichType)) {
            reason = "scriptpubkey";
            return false;
        }

        if (whichType == TX_NULL_DATA)
            nDataOut++;
        else if ((whichType == TX_MULTISIG) && (!fIsBareMultisigStd)) {
            reason = "bare-multisig";
            return false;
        } else if (CheckOutput(txout)) {
            reason = "dust";
            return false;
        }
    }

    // only one OP_RETURN txout is permitted
    if (nDataOut > 1) {
        reason = "multi-op-return";
        return false;
    }

    return true;
}

bool CStandardPolicy::CheckTxWithInputs(const CTransaction& tx, const CCoinsViewCache& mapInputs) const
{
    if (tx.IsCoinBase())
        return true; // Coinbases don't use vin normally

    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        const CTxOut& prev = mapInputs.GetOutputFor(tx.vin[i]);

        std::vector<std::vector<unsigned char> > vSolutions;
        txnouttype whichType;
        // get the scriptPubKey corresponding to this input:
        const CScript& prevScript = prev.scriptPubKey;
        if (!Solver(prevScript, whichType, vSolutions))
            return false;
        int nArgsExpected = ScriptSigArgsExpected(whichType, vSolutions);
        if (nArgsExpected < 0)
            return false;

        // Transactions with extra stuff in their scriptSigs are
        // non-standard. Note that this EvalScript() call will
        // be quick, because if there are any operations
        // beside "push data" in the scriptSig
        // CheckScript() will have already returned false
        // and this method isn't called.
        std::vector<std::vector<unsigned char> > stack;
        if (!EvalScript(stack, tx.vin[i].scriptSig, false, BaseSignatureChecker()))
            return false;

        if (whichType == TX_SCRIPTHASH)
        {
            if (stack.empty())
                return false;
            CScript subscript(stack.back().begin(), stack.back().end());
            std::vector<std::vector<unsigned char> > vSolutions2;
            txnouttype whichType2;
            if (Solver(subscript, whichType2, vSolutions2))
            {
                int tmpExpected = ScriptSigArgsExpected(whichType2, vSolutions2);
                if (tmpExpected < 0)
                    return false;
                nArgsExpected += tmpExpected;
            }
            else
            {
                // Any other Script with less than 15 sigops OK:
                unsigned int sigops = subscript.GetSigOpCount(true);
                // ... extra data left on the stack after execution is OK, too:
                return (sigops <= MAX_P2SH_SIGOPS);
            }
        }

        if (stack.size() != (unsigned int)nArgsExpected)
            return false;
    }

    return true;
}

CAmount CStandardPolicy::GetMinRelayFee(const CTransaction& tx, unsigned int nBytes, bool fAllowFree) const
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

bool CStandardPolicy::AcceptTxPoolPreInputs(CTxMemPool& pool, CValidationState& state, const CTransaction& tx) const
{
    // Rather not work on nonstandard transactions (unless -testnet/-regtest)
    std::string reason;
    if (!CheckTxPreInputs(tx, reason))
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

bool CStandardPolicy::AcceptTxWithInputs(CTxMemPool& pool, CValidationState& state, const CTransaction& tx, CCoinsViewCache& view) const
{
    // Check for non-standard pay-to-script-hash in inputs
    if (!CheckTxWithInputs(tx, view))
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

bool CStandardPolicy::AcceptMemPoolEntry(CTxMemPool& pool, CValidationState& state, CTxMemPoolEntry& entry, CCoinsViewCache& view, bool& fRateLimit) const
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

bool CStandardPolicy::RateLimitTx(CTxMemPool& pool, CValidationState& state, CTxMemPoolEntry& entry, CCoinsViewCache& view) const
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
