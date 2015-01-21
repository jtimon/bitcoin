// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// NOTE: This file is intended to be customised by the end user, and includes only local node policy logic

#include "policy.h"

#include "chainparams.h"
#include "coins.h"
#include "consensus/validation.h"
#include "primitives/transaction.h"
#include "script/standard.h"
#include "tinyformat.h"
#include "ui_interface.h"
#include "utilmoneystr.h"
#include "utilstrencodings.h"

static const unsigned int MAX_OP_RETURN_RELAY = 40; //! bytes
static const std::string defaultMinRelayTxFee = "0.00001000";

/** Declaration of Standard Policy implementing CPolicy */
class CStandardPolicy : public CPolicy
{
protected:
    unsigned nMaxDatacarrierBytes;
    bool fIsBareMultisigStd;
public:
    CStandardPolicy() : nMaxDatacarrierBytes(MAX_OP_RETURN_RELAY),
                        fIsBareMultisigStd(true) {};

    virtual void InitFromArgs(const std::map<std::string, std::string>&);
    virtual bool ValidateScript(const CScript&, txnouttype&) const;
    // "Dust" is defined in terms of CTransaction::minRelayTxFee,
    // which has units satoshis-per-kilobyte.
    // If you'd pay more than 1/3 in fees
    // to spend something, then we consider it dust.
    // A typical txout is 34 bytes big, and will
    // need a CTxIn of at least 148 bytes to spend:
    // so dust is a txout less than 546 satoshis 
    // with default minRelayTxFee.
    virtual bool ValidateOutput(const CTxOut& txout) const;
    virtual bool ValidateFee(const CAmount&, size_t) const;
    virtual bool ValidateTx(const CTransaction&, CValidationState&) const;
    /**
     * Check transaction inputs to mitigate two
     * potential denial-of-service attacks:
     * 
     * 1. scriptSigs with extra data stuffed into them,
     *    not consumed by scriptPubKey (or P2SH script)
     * 2. P2SH scripts with a crazy number of expensive
     *    CHECKSIG/CHECKMULTISIG operations
     */
    virtual bool ValidateTxInputs(const CTransaction& tx, const CCoinsViewEfficient& mapInputs) const;
};

/** Default Policy for testnet and regtest */
class CTestPolicy : public CStandardPolicy 
{
public:
    virtual bool ValidateTx(const CTransaction& tx, std::string& reason) const
    {
        return true;
    }
    virtual bool ValidateTxInputs(const CTransaction& tx, const CCoinsViewEfficient& mapInputs) const
    {
        return true;
    }
};

/** Global variables and their interfaces */

static CStandardPolicy standardPolicy;
static CTestPolicy testPolicy;

static CPolicy* pCurrentPolicy = 0;

CPolicy& Policy(std::string policy)
{
    if (policy == "standard")
        return standardPolicy;
    else if (policy == "test")
        return testPolicy;
    throw std::runtime_error(strprintf(_("Unknown policy '%s'"), policy));
}

void SelectPolicy(std::string policy)
{
    pCurrentPolicy = &Policy(policy);
}

const CPolicy& Policy()
{
    assert(pCurrentPolicy);
    return *pCurrentPolicy;
}

std::string GetPolicyUsageStr()
{
    std::string strUsage = "";
    strUsage += "  -datacarrier           " + strprintf(_("Relay and mine data carrier transactions (default: %u)"), 1) + "\n";
    strUsage += "  -datacarriersize       " + strprintf(_("Maximum size of data in data carrier transactions we relay and mine (default: %u)"), MAX_OP_RETURN_RELAY) + "\n";
    strUsage += "  -minrelaytxfee=<amt>   " + strprintf(_("Fees (in BTC/Kb) smaller than this are considered zero fee for relaying (default: %s)"), defaultMinRelayTxFee) + "\n";
    strUsage += "  -policy                " + strprintf(_("Select a specific type of policy (default: %s)"), "standard") + "\n";
    return strUsage;
}

void InitPolicyFromArgs(const std::map<std::string, std::string>& mapArgs)
{
    SelectPolicy(GetArg("-policy", Params().RequireStandard() ? "standard" : "test", mapArgs));
    pCurrentPolicy->InitFromArgs(mapArgs);
}

/** CStandardPolicy implementation */

void CStandardPolicy::InitFromArgs(const std::map<std::string, std::string>& mapArgs)
{
    if (GetArg("-datacarrier", true, mapArgs))
        nMaxDatacarrierBytes = GetArg("-datacarriersize", nMaxDatacarrierBytes, mapArgs);
    else
        nMaxDatacarrierBytes = 0;
<<<<<<< HEAD
    // Fee-per-kilobyte amount considered the same as "free"
    // If you are mining, be careful setting this:
    // if you set it to zero then
    // a transaction spammer can cheaply fill blocks using
    // 1-satoshi-fee transactions. It should be set above the real
    // cost to you of processing a transaction.
    std::string strRelayFee = GetArg("-minrelaytxfee", defaultMinRelayTxFee, mapArgs);
    CAmount n = 0;
    if (ParseMoney(strRelayFee, n) && MoneyRange(n))
        PolicyGlobal::minRelayTxFee = CFeeRate(n);
    else
        throw std::runtime_error(strprintf(_("Invalid amount for -minrelaytxfee=<amount>: '%s'"), strRelayFee));
=======
    fIsBareMultisigStd = GetArg("-permitbaremultisig", fIsBareMultisigStd, mapArgs);
>>>>>>> 2c95a08... Policy: Refactor: main::IsStandardTx(CTransaction, string) -> CPolicy::ValidateTx(CTransaction, CValidationState)
}

bool CStandardPolicy::ValidateScript(const CScript& scriptPubKey, txnouttype& whichType) const
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

bool CStandardPolicy::ValidateOutput(const CTxOut& txout) const
{
    size_t nSize = txout.GetSerializeSize(SER_DISK,0) + 148u;
    return (txout.nValue < 3 *  PolicyGlobal::minRelayTxFee.GetFee(nSize));
}

bool CStandardPolicy::ValidateFee(const CAmount& nFees, size_t nSize) const
{
    return nFees < PolicyGlobal::minRelayTxFee.GetFee(nSize);
}

bool CStandardPolicy::ValidateTx(const CTransaction& tx, CValidationState& state) const
{
    if (tx.nVersion > CTransaction::CURRENT_VERSION || tx.nVersion < 1)
        return state.DoS(0, false, REJECT_NONSTANDARD, "version");

    // Extremely large transactions with lots of inputs can cost the network
    // almost as much to process as they cost the sender in fees, because
    // computing signature hashes is O(ninputs*txsize). Limiting transactions
    // to MAX_STANDARD_TX_SIZE mitigates CPU exhaustion attacks.
    unsigned int sz = tx.GetSerializeSize(SER_NETWORK, CTransaction::CURRENT_VERSION);
    if (sz >= MAX_STANDARD_TX_SIZE)
        return state.DoS(0, false, REJECT_NONSTANDARD, "tx-size");

    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        // Biggest 'standard' txin is a 15-of-15 P2SH multisig with compressed
        // keys. (remember the 520 byte limit on redeemScript size) That works
        // out to a (15*(33+1))+3=513 byte redeemScript, 513+1+15*(73+1)+3=1627
        // bytes of scriptSig, which we round off to 1650 bytes for some minor
        // future-proofing. That's also enough to spend a 20-of-20
        // CHECKMULTISIG scriptPubKey, though such a scriptPubKey is not
        // considered standard)
        if (tx.vin[i].scriptSig.size() > 1650)
            return state.DoS(0, false, REJECT_NONSTANDARD, "scriptsig-size");

        if (!tx.vin[i].scriptSig.IsPushOnly())
            return state.DoS(0, false, REJECT_NONSTANDARD, "scriptsig-not-pushonly");
    }

    unsigned int nDataOut = 0;
    txnouttype whichType;
    for (unsigned int i = 0; i < tx.vout.size(); i++) {
        if (!ValidateScript(tx.vout[i].scriptPubKey, whichType))
            return state.DoS(0, false, REJECT_NONSTANDARD, "scriptpubkey");

        if (whichType == TX_NULL_DATA)
            nDataOut++;
        else if ((whichType == TX_MULTISIG) && (!fIsBareMultisigStd))
            return state.DoS(0, false, REJECT_NONSTANDARD, "bare-multisig");
        else if (ValidateOutput(tx.vout[i]))
            return state.DoS(0, false, REJECT_NONSTANDARD, "dust");
    }

    // only one OP_RETURN txout is permitted
    if (nDataOut > 1)
        return state.DoS(0, false, REJECT_NONSTANDARD, "multi-op-return");

    return true;
}

bool CStandardPolicy::ValidateTxInputs(const CTransaction& tx, const CCoinsViewEfficient& mapInputs) const
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
        // CPolicy::ValidateScript() will have already returned false
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
