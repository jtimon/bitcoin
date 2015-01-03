// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POLICY_H
#define BITCOIN_POLICY_H

#include "amount.h"
#include "script/standard.h"

class CCoinsViewCache;
class CFeeRate;
class CTransaction;
class CTxMemPool;
class CTxMemPoolEntry;
class CValidationState;

extern CFeeRate minRelayTxFee;

class CNodePolicyBase
{
public:
    virtual bool AcceptTxPoolPreInputs(CTxMemPool&, CValidationState&, const CTransaction&) = 0;
    virtual bool AcceptTxWithInputs(CTxMemPool&, CValidationState&, const CTransaction&, CCoinsViewCache&) = 0;
    virtual bool AcceptMemPoolEntry(CTxMemPool&, CValidationState&, CTxMemPoolEntry&, CCoinsViewCache&, bool& fRateLimit) = 0;
    virtual bool RateLimitTx(CTxMemPool&, CValidationState&, CTxMemPoolEntry&, CCoinsViewCache&) = 0;
};

class CNodePolicy : CNodePolicyBase
{
public:
    bool fRequireStandardTx;

    CNodePolicy() : fRequireStandardTx(true) { };

    virtual bool IsStandardScript(const CScript& scriptPubKey, txnouttype& whichType);

    /** Check for standard transaction types
     * @return True if all outputs (scriptPubKeys) use only standard transaction forms
     */
    virtual bool IsStandardTx(const CTransaction&, std::string& reason);
    /** 
     * Check for standard transaction types
     * @param[in] mapInputs    Map of previous transactions that have outputs we're spending
     * @return True if all inputs (scriptSigs) use only standard transaction forms
     */
    virtual bool AreInputsStandard(const CTransaction&, const CCoinsViewCache& mapInputs);

    virtual CAmount GetMinRelayFee(const CTransaction&, unsigned int nBytes, bool fAllowFree);

    virtual bool AcceptTxPoolPreInputs(CTxMemPool&, CValidationState&, const CTransaction&);
    virtual bool AcceptTxWithInputs(CTxMemPool&, CValidationState&, const CTransaction&, CCoinsViewCache&);
    virtual bool AcceptMemPoolEntry(CTxMemPool&, CValidationState&, CTxMemPoolEntry&, CCoinsViewCache&, bool& fRateLimit);
    virtual bool RateLimitTx(CTxMemPool&, CValidationState&, CTxMemPoolEntry&, CCoinsViewCache&);
};

extern CNodePolicy policy;

void InitPolicyFromCommandLine();

#endif // BITCOIN_POLICY_H
