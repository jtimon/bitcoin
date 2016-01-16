// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_BITCOINCONSENSUS_H
#define BITCOIN_BITCOINCONSENSUS_H

#include "consensus/interfaces.h"
#include "consensus/params.h"

#if defined(BUILD_BITCOIN_INTERNAL) && defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
  #if defined(_WIN32)
    #if defined(DLL_EXPORT)
      #if defined(HAVE_FUNC_ATTRIBUTE_DLLEXPORT)
        #define EXPORT_SYMBOL __declspec(dllexport)
      #else
        #define EXPORT_SYMBOL
      #endif
    #endif
  #elif defined(HAVE_FUNC_ATTRIBUTE_VISIBILITY)
    #define EXPORT_SYMBOL __attribute__ ((visibility ("default")))
  #endif
#elif defined(MSC_VER) && !defined(STATIC_LIBBITCOINCONSENSUS)
  #define EXPORT_SYMBOL __declspec(dllimport)
#endif

#ifndef EXPORT_SYMBOL
  #define EXPORT_SYMBOL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define BITCOINCONSENSUS_API_VER 0

typedef enum bitcoinconsensus_error_t
{
    bitcoinconsensus_ERR_OK = 0,
    bitcoinconsensus_ERR_TX_INDEX,
    bitcoinconsensus_ERR_TX_SIZE_MISMATCH,
    bitcoinconsensus_ERR_TX_DESERIALIZE,
} bitcoinconsensus_error;

/** Script verification flags */
enum
{
    bitcoinconsensus_SCRIPT_FLAGS_VERIFY_NONE                = 0,
    bitcoinconsensus_SCRIPT_FLAGS_VERIFY_P2SH                = (1U << 0), // evaluate P2SH (BIP16) subscripts
    bitcoinconsensus_SCRIPT_FLAGS_VERIFY_DERSIG              = (1U << 2), // enforce strict DER (BIP66) compliance
    bitcoinconsensus_SCRIPT_FLAGS_VERIFY_CHECKLOCKTIMEVERIFY = (1U << 9), // enable CHECKLOCKTIMEVERIFY (BIP65)
};

/// Returns 1 if the input nIn of the serialized transaction pointed to by
/// txTo correctly spends the scriptPubKey pointed to by scriptPubKey under
/// the additional constraints specified by flags.
/// If not NULL, err will contain an error/success code for the operation
EXPORT_SYMBOL int bitcoinconsensus_verify_script(const unsigned char *scriptPubKey, unsigned int scriptPubKeyLen,
                                    const unsigned char *txTo        , unsigned int txToLen,
                                    unsigned int nIn, unsigned int flags, bitcoinconsensus_error* err);

EXPORT_SYMBOL int bitcoinconsensus_verify_header(const unsigned char* blockHeader, unsigned int blockHeaderLen, const Consensus::Params& consensusParams, int64_t nTime, void* pindexPrev, const Consensus::BlockIndexInterface& indexInterface, bitcoinconsensus_error* err);

EXPORT_SYMBOL unsigned int bitcoinconsensus_get_flags(const unsigned char* blockHeader, unsigned int blockHeaderLen, const Consensus::Params& consensusParams, void* pindexPrev, const Consensus::BlockIndexInterface& indexInterface, bitcoinconsensus_error* err);

EXPORT_SYMBOL int bitcoinconsensus_verify_tx(const unsigned char* tx, unsigned int txLen, void* pindexPrev, const Consensus::BlockIndexInterface& indexInterface, void* inputs, const Consensus::CoinsIndexInterface& inputsInterface, const int64_t nHeight, const int64_t nSpendHeight, const int64_t nLockTimeCutoff, unsigned int flags, int fScriptChecks, int cacheStore, uint64_t& nFees, int64_t& nSigOps, bitcoinconsensus_error* err);

/**
 * Fully verify a CBlock. This is exposed mostly for testing.
 * This is the potentially more optimal (and thus recommended) way
 * to use libbitcoinconsensus to verify per-block instead:
 * 1) Call bitcoinconsensus_verify_header()
 * 2) Call bitcoinconsensus_get_flags()
 * 3) TODO Call Consensus::CheckBlock(), with fCheckPOW=0 and flags=<result_in_step2>
 * 4) Check subsidy/no-coinbase-money-creation
 * 5) foreach tx: Parallelize tx checking by calling bitcoinconsensus_verify_tx() with fScriptChecks=0 and flags=<result_in_step2>
 * 6) foreach tx foreach input:Parallelize script checking by calling bitcoinconsensus_verify_script() with flags=<result_in_step2>
 */
EXPORT_SYMBOL int bitcoinconsensus_verify_block(const unsigned char* block, unsigned int blockLen, const Consensus::Params& consensusParams, int64_t nTime, const int64_t nSpendHeight, void* pindexPrev, const Consensus::BlockIndexInterface& indexInterface, const void* inputs, const Consensus::CoinsIndexInterface& inputsInterface, bool fNewBlock, bool fScriptChecks, bool cacheStore, bool fCheckPOW, bitcoinconsensus_error* err);

EXPORT_SYMBOL unsigned int bitcoinconsensus_version();

#ifdef __cplusplus
} // extern "C"
#endif

#undef EXPORT_SYMBOL

#endif // BITCOIN_BITCOINCONSENSUS_H
