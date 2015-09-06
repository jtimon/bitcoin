// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// NOTE: This file is intended to be customised by the end user, and includes only local node policy logic

#include "policy/policy.h"

#include "chainparams.h"
#include "main.h"
#include "tinyformat.h"
#include "util.h"
#include "utilmoneystr.h"
#include "utilstrencodings.h"

#include <boost/foreach.hpp>

/** CStandardPolicy initialization */

std::vector<std::pair<std::string, std::string> > CStandardPolicy::GetOptionsHelp() const
{
    std::vector<std::pair<std::string, std::string> > optionsHelp;
    optionsHelp.push_back(std::make_pair("-permitbaremultisig", strprintf(_("Relay non-P2SH multisig (default: %u)"), fIsBareMultisigStd)));
    optionsHelp.push_back(std::make_pair("-acceptnonstdtxn", strprintf(_("Relay and mine \"non-standard\" transactions (default: %u)"), Params(CBaseChainParams::MAIN).RequireStandard())));
    optionsHelp.push_back(std::make_pair("-minrelaytxfee=<amt>", strprintf(_("Fees (in %s/Kb) smaller than this are considered zero fee for relaying (default: %s)"), CURRENCY_UNIT, FormatMoney(minRelayFee.GetFeePerK()))));
    return optionsHelp;
}

void CStandardPolicy::InitFromArgs(const std::map<std::string, std::string>& mapArgs)
{
    fIsBareMultisigStd = GetBoolArg("-permitbaremultisig", fIsBareMultisigStd, mapArgs);
    fAcceptNonStdTxn = GetBoolArg("-acceptnonstdtxn", !Params().RequireStandard(), mapArgs);
    InitMinRelayFee(ParseAmountFromArgs("-minrelaytxfee", minRelayFee.GetFee(1000), mapArgs));
}

CStandardPolicy::CStandardPolicy(const CAmount& nMinRelayFeePerK, bool fIsBareMultisigStdIn, bool fAcceptNonStdTxnIn) :
    CBlockPolicyEstimator(nMinRelayFeePerK),
    fIsBareMultisigStd(fIsBareMultisigStdIn),
    fAcceptNonStdTxn(fAcceptNonStdTxnIn)
{
}

/** CStandardPolicy implementation */

bool CStandardPolicy::ApproveScript(const CScript& scriptPubKey, txnouttype& whichType) const
{
    std::vector<std::vector<unsigned char> > vSolutions;
    if (!Solver(scriptPubKey, whichType, vSolutions))
        return false;

    if (whichType == TX_MULTISIG)
    {
        unsigned char m = vSolutions.front()[0];
        unsigned char n = vSolutions.back()[0];
        // Support up to x-of-3 multisig txns as standard
        if (n < 1 || n > 3)
            return false;
        if (m < 1 || m > n)
            return false;
    }

    return whichType != TX_NONSTANDARD;
}

bool CStandardPolicy::ApproveScript(const CScript& scriptPubKey) const
{
    txnouttype whichType;
    return ApproveScript(scriptPubKey, whichType);
}

bool CStandardPolicy::ApproveTx(const CTransaction& tx, std::string& reason) const
{
    if (fAcceptNonStdTxn)
        return true;

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
        if (!ApproveScript(txout.scriptPubKey, whichType)) {
            reason = "scriptpubkey";
            return false;
        }

        if (whichType == TX_NULL_DATA)
            nDataOut++;
        else if ((whichType == TX_MULTISIG) && (!fIsBareMultisigStd)) {
            reason = "bare-multisig";
            return false;
        } else if (txout.IsDust(::minRelayTxFee)) {
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

bool CStandardPolicy::ApproveTxInputs(const CTransaction& tx, const CCoinsViewCache& mapInputs) const
{
    if (fAcceptNonStdTxn)
        return true;

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
        // IsStandardTx() will have already returned false
        // and this method isn't called.
        std::vector<std::vector<unsigned char> > stack;
        if (!EvalScript(stack, tx.vin[i].scriptSig, SCRIPT_VERIFY_NONE, BaseSignatureChecker()))
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
