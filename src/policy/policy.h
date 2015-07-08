// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POLICY_H
#define BITCOIN_POLICY_H

#include "consensus/consensus.h"
#include "script/interpreter.h"
#include "script/standard.h"

#include <map>
#include <string>

class CCoinsViewCache;

/** Default for -blockmaxsize and -blockminsize, which control the range of sizes the mining code will create **/
static const unsigned int DEFAULT_BLOCK_MAX_SIZE = 750000;
static const unsigned int DEFAULT_BLOCK_MIN_SIZE = 0;
/** Default for -blockprioritysize, maximum space for zero/low-fee transactions **/
static const unsigned int DEFAULT_BLOCK_PRIORITY_SIZE = 50000;
/** The maximum size for transactions we're willing to relay/mine */
static const unsigned int MAX_STANDARD_TX_SIZE = 100000;
/** Maximum number of signature check operations in an IsStandard() P2SH script */
static const unsigned int MAX_P2SH_SIGOPS = 15;
/** The maximum number of sigops we're willing to relay/mine in a single tx */
static const unsigned int MAX_STANDARD_TX_SIGOPS = MAX_BLOCK_SIGOPS/5;
/**
 * Standard script verification flags that standard transactions will comply
 * with. However scripts violating these flags may still be present in valid
 * blocks and we must accept those blocks.
 */
static const unsigned int STANDARD_SCRIPT_VERIFY_FLAGS = MANDATORY_SCRIPT_VERIFY_FLAGS |
                                                         SCRIPT_VERIFY_DERSIG |
                                                         SCRIPT_VERIFY_STRICTENC |
                                                         SCRIPT_VERIFY_MINIMALDATA |
                                                         SCRIPT_VERIFY_NULLDUMMY |
                                                         SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS |
                                                         SCRIPT_VERIFY_CLEANSTACK |
                                                         SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;

/** For convenience, standard but not mandatory verify flags. */
static const unsigned int STANDARD_NOT_MANDATORY_VERIFY_FLAGS = STANDARD_SCRIPT_VERIFY_FLAGS & ~MANDATORY_SCRIPT_VERIFY_FLAGS;

/**
 * \class CPolicy
 * Generic interface class for policy.
 */
class CPolicy
{
public:
    virtual ~CPolicy() {};
    /**
     * @param argMap a map with options to read from.
     * @return a formatted HelpMessage string with the policy options
     */
    virtual std::vector<std::pair<std::string, std::string> > GetOptionsHelp() const
    {
        std::vector<std::pair<std::string, std::string> > optionsHelp;
        return optionsHelp;
    }
    /**
     * @param argMap a map with options to read from.
     * @return a formatted HelpMessage string with the policy options
     */
    virtual void InitFromArgs(const std::map<std::string, std::string>& argMap) {};
    bool ApproveScript(const CScript& scriptPubKey) const { return true; };
    /**
     * Check for standard transaction types
     * @return True if all outputs (scriptPubKeys) use only standard transaction forms
     */
    virtual bool ApproveTx(const CTransaction& tx, std::string& reason) const { return true; };
    /**
     * Check for standard transaction types
     * @param[in] mapInputs    Map of previous transactions that have outputs we're spending
     * @return True if all inputs (scriptSigs) use only standard transaction forms
     */
    virtual bool ApproveTxInputs(const CTransaction& tx, const CCoinsViewCache& mapInputs) const { return true; };
    /**
     * @param txout the CTxOut being considered
     * @return the minimum acceptable nValue for this CTxOut.
     */
    virtual CAmount GetMinAmount(const CTxOut& txout) const { return true; };
    /**
     * @param txout the CTxOut being considered
     * @return True if the CTxOut has an acceptable nValue.
     */
    virtual bool ApproveOutputAmount(const CTxOut& txout) const { return true; };
};

/**
 * \class CStandardPolicy
 * Standard implementation of CPolicy.
 */
class CStandardPolicy : public CPolicy
{
protected:
    bool fIsBareMultisigStd;
    bool fAcceptNonStdTxn;

    bool ApproveScript(const CScript&, txnouttype&) const;
public:
    CStandardPolicy(bool fIsBareMultisigStdIn=true, 
                    bool fAcceptNonStdTxnIn=false);
    virtual std::vector<std::pair<std::string, std::string> > GetOptionsHelp() const;
    virtual void InitFromArgs(const std::map<std::string, std::string>&);
    virtual bool ApproveScript(const CScript& scriptPubKey) const;
    virtual bool ApproveTx(const CTransaction& tx, std::string& reason) const;
    /**
     * Check transaction inputs to mitigate two
     * potential denial-of-service attacks:
     * 
     * 1. scriptSigs with extra data stuffed into them,
     *    not consumed by scriptPubKey (or P2SH script)
     * 2. P2SH scripts with a crazy number of expensive
     *    CHECKSIG/CHECKMULTISIG operations
     *
     * Check transaction inputs, and make sure any
     * pay-to-script-hash transactions are evaluating IsStandard scripts
     * 
     * Why bother? To avoid denial-of-service attacks; an attacker
     * can submit a standard HASH... OP_EQUAL transaction,
     * which will get accepted into blocks. The redemption
     * script can be anything; an attacker could use a very
     * expensive-to-check-upon-redemption script like:
     *   DUP CHECKSIG DROP ... repeated 100 times... OP_1
     */
    virtual bool ApproveTxInputs(const CTransaction& tx, const CCoinsViewCache& mapInputs) const;
    /**
     * "Dust" is defined in terms of CStandardPolicy::minRelayTxFee,
     * which has units satoshis-per-kilobyte.
     * If you'd pay more than 1/3 in fees
     * to spend something, then we consider it dust.
     * A typical txout is 34 bytes big, and will
     * need a CTxIn of at least 148 bytes to spend:
     * so dust is a txout less than 546 satoshis 
     * with default minRelayTxFee.
     */
    virtual CAmount GetMinAmount(const CTxOut& txout) const;
    virtual bool ApproveOutputAmount(const CTxOut& txout) const;
};

namespace Policy {

/**
 * Returns a new CPolicy* with the parameters specified. The caller has to delete the object.
 * @param defaultPolicy the string selecting the policy.
 * @param mapArgs [optional] a map with values for policy options (mapArgs["-policy"] overrides defaultPolicy).
 * @return the newly created CPolicy*.
 * @throws a std::runtime_error if the policy is not supported.
 */
CPolicy* Factory(const std::string& defaultPolicy, const std::map<std::string, std::string>& mapArgs);
CPolicy* Factory(const std::string& policy);
/**
 * Append a help string for the options of the selected policy.
 * @param strUsage a formatted HelpMessage string with policy options
 * is appended to this string
 * @param selectedPolicy select a policy to show its options
 */
void AppendHelpMessages(std::string& strUsage, const std::string& selectedPolicy);

/** Supported policies */
static const std::string STANDARD = "standard";
static const std::string TEST = "test";

} // namespace Policy

#endif // BITCOIN_POLICY_H
