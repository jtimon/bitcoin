// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_CONSENSUS_H
#define BITCOIN_CONSENSUS_CONSENSUS_H

#include "consensus/types.h"

#include <stdint.h>

class CBlock;
class CBlockHeader;
class CBlockIndex;
class CCoinsViewCache;
class CTransaction;
class CValidationState;
class uint256;

/** The maximum allowed size for a serialized block, in bytes (network rule) */
static const unsigned int MAX_BLOCK_SIZE = 1000000;
/** The maximum allowed number of signature check operations in a block (network rule) */
static const unsigned int MAX_BLOCK_SIGOPS = MAX_BLOCK_SIZE/50;
/** Coinbase transaction outputs can only be spent after this number of new blocks (network rule) */
static const int COINBASE_MATURITY = 100;
/** Threshold for nLockTime: below this value it is interpreted as block number, otherwise as UNIX timestamp. */
static const unsigned int LOCKTIME_THRESHOLD = 500000000; // Tue Nov  5 00:53:20 1985 UTC

/**
 * Consensus validations:
 * Check_ means checking everything possible with the data provided.
 * Verify_ means all data provided was enough for this level and its "consensus-verified".
 */
namespace Consensus {

class Params;

/** Transaction validation functions */

/**
 * Fully verify a transaction.
 */
bool VerifyTx(const CTransaction& tx, CValidationState &state, int nBlockHeight, int64_t nBlockTime, const CCoinsViewCache& inputs, int nSpendHeight, bool cacheStore, unsigned int flags);
/**
 * Context-independent CTransaction validity checks
 */
bool CheckTx(const CTransaction& tx, CValidationState& state);
/**
 * Check whether all inputs of this transaction are valid (no double spends and amounts)
 * This does not modify the UTXO set. This does not check scripts and sigs.
 * Preconditions: tx.IsCoinBase() is false.
 */
bool CheckTxInputs(const CTransaction& tx, CValidationState& state, const CCoinsViewCache& inputs, int nSpendHeight);
/**
 * Preconditions: tx.IsCoinBase() is false.
 * Check whether all inputs of this transaction are valid (scripts and sigs)
 * This does not modify the UTXO set. This does not check double spends and amounts.
 * This is the more expensive consensus check for a transaction, do it last.
 */
bool CheckTxInputsScripts(const CTransaction& tx, CValidationState& state, const CCoinsViewCache& inputs, bool cacheStore, unsigned int flags);

/** Block header validation functions */

bool VerifyBlockHeader(const CBlockHeader& block, CValidationState& state, const Params& consensusParams, int64_t nTime, const CBlockIndexBase* pindexPrev, PrevIndexGetter indexGetter);
/**
 * Context-independent CBlockHeader validity checks
 */
bool CheckBlockHeader(const CBlockHeader& block, CValidationState& state, const Params& consensusParams, int64_t nTime, bool fCheckPOW = true);
/**
 * Context-dependent CBlockHeader validity checks
 */
bool ContextualCheckBlockHeader(const CBlockHeader& block, CValidationState& state, const Params& consensusParams, const CBlockIndexBase* pindexPrev, PrevIndexGetter);

/** Block validation functions */

/**
 * Fully verify a block.
 */
bool VerifyBlock(const CBlock&, CValidationState&, const Consensus::Params&, int64_t nTime, const CBlockIndexBase*, PrevIndexGetter);
/**
 * Context-independent CBlock validity checks
 */
bool CheckBlock(const CBlock& block, CValidationState& state, const Params& params, int64_t nTime, bool fCheckPOW = true, bool fCheckMerkleRoot = true);
/**
 * Context-dependent CBlock validity checks
 */
bool ContextualCheckBlock(const CBlock& block, CValidationState& state, const Params& params, const CBlockIndexBase* pindexPrev, PrevIndexGetter);

/** Block validation utilities */

/**
 * BIP16 didn't become active until Apr 1 2012
 * Starts enforcing the DERSIG (BIP66) rules, for block.nVersion=3 blocks, when 75% of the network has upgraded
 */
unsigned int GetFlags(const CBlock&, const Consensus::Params&, CBlockIndexBase* pindex, PrevIndexGetter);
/**
 * Do not allow blocks that contain transactions which 'overwrite' older transactions,
 * unless those are already completely spent.
 * If such overwrites are allowed, coinbases and transactions depending upon those
 * can be duplicated to remove the ability to spend the first instance -- even after
 * being sent to another address.
 * See BIP30 and http://r6.ca/blog/20120206T005236Z.html for more information.
 * This logic is not necessary for memory pool transactions, as AcceptToMemoryPool
 * already refuses previously-known transaction ids entirely.
 * This rule was originally applied all blocks whose timestamp was after March 15, 2012, 0:00 UTC.
 * Now that the whole chain is irreversibly beyond that time it is applied to all blocks except the
 * two in the chain that violate it. This prevents exploiting the issue against nodes in their
 * initial block download.
 */
bool EnforceBIP30(const CBlock& block, CValidationState& state, const CBlockIndex* pindexPrev, const CCoinsViewCache& inputs);

} // namespace Consensus

/** Transaction validation utility functions */

bool CheckFinalTx(const CTransaction &tx, int nBlockHeight, int64_t nBlockTime);
/**
 * Count ECDSA signature operations the old-fashioned (pre-0.6) way
 * @return number of sigops this transaction's outputs will produce when spent
 * @see CTransaction::FetchInputs
 */
unsigned int GetLegacySigOpCount(const CTransaction& tx);
/**
 * Count ECDSA signature operations in pay-to-script-hash inputs.
 * 
 * @param[in] mapInputs Map of previous transactions that have outputs we're spending
 * @return maximum number of sigops required to validate this transaction's inputs
 * @see CTransaction::FetchInputs
 */
unsigned int GetP2SHSigOpCount(const CTransaction& tx, const CCoinsViewCache& mapInputs);
/**
 * Count ECDSA signature operations.
 * @see GetLegacySigOpCount and GetP2SHSigOpCount
 */
unsigned int GetSigOpCount(const CTransaction&, const CCoinsViewCache&);

/** Block header validation utility functions */

int64_t GetMedianTimePast(const CBlockIndexBase* pindex, PrevIndexGetter);
uint32_t GetNextWorkRequired(const CBlockIndexBase* pindexLast, const CBlockHeader *pblock, const Consensus::Params&, PrevIndexGetter);
uint32_t CalculateNextWorkRequired(const CBlockIndexBase* pindexLast, int64_t nFirstBlockTime, const Consensus::Params&);
/** Check whether a block hash satisfies the proof-of-work requirement specified by nBits */
bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params&);

#endif // BITCOIN_CONSENSUS_CONSENSUS_H
