// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POW_H
#define BITCOIN_POW_H

#include "consensus/params.h"

#include <stdint.h>

class CBlockHeader;
class CBlockIndex;
class CKeyStore;
class CValidationState;
class uint256;

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params&);
unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params&);

bool CheckBlockHeader(const CBlockHeader& block, CValidationState& state, const Consensus::Params& params);
bool MaybeGenerateProof(const Consensus::Params& params, CBlockHeader* pblock, const CKeyStore* pkeystore, uint64_t& nTries);
/** Only for testing pow chains, uses a an empty CKeyStore. */ 
bool GenerateProof(const Consensus::Params& params, CBlockHeader* pblock);
void ResetProof(CBlockHeader* pblock);

#endif // BITCOIN_POW_H
