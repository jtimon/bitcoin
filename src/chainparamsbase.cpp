// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparamsbase.h>

#include <tinyformat.h>
#include <util/system.h>
#include <util/memory.h>

#include <assert.h>

const std::string CBaseChainParams::MAIN = "main";
const std::string CBaseChainParams::TESTNET = "test";
const std::string CBaseChainParams::SIGNET = "signet";
const std::string CBaseChainParams::REGTEST = "regtest";

void SetupChainParamsBaseOptions()
{
    gArgs.AddArg("-chain=<chain>", "Use the chain <chain> (default: main). Reserved values: main, test, regtest. With any other value, a custom chain is used.", ArgsManager::ALLOW_ANY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-regtest", "Enter regression test mode, which uses a special chain in which blocks can be solved instantly. "
                 "This is intended for regression testing tools and app development. Equivalent to -chain=regtest.", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-testnet", "Use the test chain. Equivalent to -chain=test.", ArgsManager::ALLOW_ANY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-signet", "Use the signet chain. Note that the network is defined by the signet_blockscript parameter", ArgsManager::ALLOW_ANY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-signet_blockscript", "Blocks must satisfy the given script to be considered valid (only for -signet networks)", ArgsManager::ALLOW_STRING, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-signet_enforcescript", "Blocks must satisfy the given script to be considered valid (this replaces -signet_blockscript, and is used for opt-in-reorg mode)", ArgsManager::ALLOW_STRING, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-signet_seednode", "Specify a seed node for the signet network (may be used multiple times to specify multiple seed nodes)", ArgsManager::ALLOW_STRING, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-vbparams=deployment:start:end", "Use given start/end times for specified version bits deployment (regtest or custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-segwitheight=<n>", "Set the activation height of segwit. -1 to disable. (regtest or custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::DEBUG_TEST);
    gArgs.AddArg("-con_nsubsidyhalvinginterval", "Number of blocks between one subsidy adjustment and the next one. Default: 150 (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-con_bip16exception", "A block hash not to validate BIP16 on. (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-con_bip34height", "Height from which BIP34 is enforced. Default: 500 (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-con_bip34hash", "Hardcoded hash for BIP34 activation corresponding to the bip34height so that bip30 checks can be saved. (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-con_bip65height", "Height from which BIP65 is enforced. Default: 1351 (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-con_bip66height", "Height from which BIP66 is enforced. Default: 1251 (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-con_powlimit", "Maximum proof of work target. Default 7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-con_npowtargettimespan", "Proof of work retargetting interval in seconds. Default: 2 weeks (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-con_npowtargetspacing", "Proof of work target for interval between blocks in seconds. Default: 600 (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-con_fpowallowmindifficultyblocks", "Whether the chain allows minimum difficulty blocks or not. Default: 1 (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-con_fpownoretargeting", "Whether the chain skips proof of work retargetting or not. Default: 1 (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-con_nminerconfirmationwindow", "Interval for BIP9 deployment activation. Default: 144 (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-con_nrulechangeactivationthreshold", "Minimum blocks to signal readiness for a chain for BIP9 activation. Default 108 (ie 75%). (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-con_nminimumchainwork", "The best chain should have at least this much work. Default: 0 (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-con_defaultassumevalid", "By default assume that the signatures in ancestors of this block are valid. Consider using -assumevalid instead. (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-ndefaultport", "The port to listen for connections on by default. Consider using -port instead of changing the default.  Default: 18444 (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-npruneafterheight", "Only start prunning after this height. Default: 1000 (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-assumed_blockchain_size", "Estimated current blockchain size (in GB) for UI purposes. Default 0 (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-assumed_chain_state_size", "Estimated current chain state size (in GB) for UI purposes. Default 0 (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-fdefaultconsistencychecks", "Whether -checkblockindex and -checkmempool are active by default or not. Consider using those options instead. Default: 1 (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-frequirestandard", "Whether standard policy rules are applied in the local mempool by default. Consider using -acceptnonstdtxn=0 instead of changing the default. Default: 0 (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-is_test_chain", "Whether it's allowed to set -acceptnonstdtxn=0 for this chain or not. It also affects the default for  -fallbackfee=0. Consider using -fallbackfee=0 instead of changing the default. Default: 1 (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-bech32_hrp", "Human readable part for bech32 addresses. See BIP173 for more info. Default: bcrt (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-pubkeyprefix", "Magic for base58 pubkeys. (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-scriptprefix", "Magic for base58 scripts. (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-secretprefix", "Magic for base58 secret keys. (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-extpubkeyprefix", "Magic for base58 external pubkeys. Default: 043587CF (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-extprvkeyprefix", "Magic for base58 external secret keys. Default: 04358394 (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-pchmessagestart", "Magic for p2p protocol. Default: FABFB5DA (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-seednode=<ip>", "Use specified node as seed node. This option can be specified multiple times to connect to multiple nodes. (custom only)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
}

static std::unique_ptr<CBaseChainParams> globalChainBaseParams;

const CBaseChainParams& BaseParams()
{
    assert(globalChainBaseParams);
    return *globalChainBaseParams;
}

std::unique_ptr<CBaseChainParams> CreateBaseChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return MakeUnique<CBaseChainParams>("", 8332);
    else if (chain == CBaseChainParams::TESTNET)
        return MakeUnique<CBaseChainParams>("testnet3", 18332);
    else if (chain == CBaseChainParams::REGTEST)
        return MakeUnique<CBaseChainParams>("regtest", 18443);
    else if (chain == CBaseChainParams::SIGNET)
        return MakeUnique<CBaseChainParams>("signet", 38332);

    return MakeUnique<CBaseChainParams>(chain, 18553);
}

void SelectBaseParams(const std::string& chain)
{
    globalChainBaseParams = CreateBaseChainParams(chain);
    gArgs.SelectConfigNetwork(chain);
}
