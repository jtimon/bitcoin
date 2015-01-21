// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// NOTE: This file is intended to be customised by the end user, and includes only local node policy logic

#include "policy.h"

#include "primitives/transaction.h"
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
public:
    CStandardPolicy() : nMaxDatacarrierBytes(MAX_OP_RETURN_RELAY) {};

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
};

/** Global variables and their interfaces */

static CStandardPolicy standardPolicy;

static CPolicy* pCurrentPolicy = 0;

CPolicy& Policy(std::string policy)
{
    if (policy == "standard")
        return standardPolicy;
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
    SelectPolicy(GetArg("-policy", "standard", mapArgs));
    pCurrentPolicy->InitFromArgs(mapArgs);
}

/** CStandardPolicy implementation */

void CStandardPolicy::InitFromArgs(const std::map<std::string, std::string>& mapArgs)
{
    if (GetArg("-datacarrier", true, mapArgs))
        nMaxDatacarrierBytes = GetArg("-datacarriersize", nMaxDatacarrierBytes, mapArgs);
    else
        nMaxDatacarrierBytes = 0;
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
