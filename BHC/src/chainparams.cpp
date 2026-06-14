// Copyright (c) 2026 BHC Developers
// Distributed under the MIT software license
// BHC Chain Parameters Implementation

#include "chainparams.h"
#include "crypto/progpow.h"
#include "crypto/mldsa.h"

#include <cstdint>
#include <cstring>

namespace bhc {
namespace chain {

static CChainParams* g_current_params = nullptr;

CChainParams::CChainParams(Network network) : m_network(network) {
    switch (network) {
        case MAIN:
            m_strNetworkID = "main";
            
            m_consensus.nSubsidyHalvingInterval = 210000;
            m_consensus.BIP34Height = 1;
            m_consensus.BIP65Height = 1;
            m_consensus.BIP66Height = 1;
            
            std::fill(m_consensus.powLimit.begin(), m_consensus.powLimit.end(), 0);
            m_consensus.powLimit[28] = 0x00;
            m_consensus.powLimit[29] = 0x0f;
            m_consensus.powLimit[30] = 0xff;
            m_consensus.powLimit[31] = 0xff;
            
            m_consensus.nPowTargetTimespan = 14 * 24 * 60 * 60;
            m_consensus.nPowTargetSpacing = 10 * 60;
            m_consensus.fPowAllowMinDifficultyBlocks = false;
            m_consensus.fPowNoRetargeting = false;
            
            m_consensus.nLWMAWindowSize = 45;
            m_consensus.nProgPoWPeriod = crypto::PROGPOW_PERIOD;
            
            // Genesis Block Parameters (Generated: 2026-03-21)
            // Hash: 3a7ea551d2d41377e210cd9f5af4b291a24d425e0f1666ada84fcf53ed0b0000
            // Nonce: 2335930
            // Message: "BHC: The Future of Digital Gold 2026 - Decentralized & Post-Quantum"
            const char* genesisHashHex = "3a7ea551d2d41377e210cd9f5af4b291a24d425e0f1666ada84fcf53ed0b0000";
            for (size_t i = 0; i < 32; ++i) {
                unsigned int byte;
                sscanf(genesisHashHex + i * 2, "%02x", &byte);
                m_consensus.hashGenesisBlock[31 - i] = static_cast<uint8_t>(byte);
            }
            
            m_consensus.fProgPowActive = true;
            m_consensus.fMLDSAActive = true;
            
            m_nDefaultPort = 18334;
            m_nRPCPort = 18332;
            
            m_magicBytes = {0x42, 0x48, 0x43, 0x01};
            
            m_base58PrefixPubKey = {0x1A};
            m_base58PrefixScript = {0x1C};
            m_bech32Hrp = "bhc";
            
            m_vSeeds = {
                {"seed1.bhc-core.org", 18334},
                {"seed2.bhc-core.org", 18334},
                {"seed3.bhc-core.org", 18334},
                {"seed.bhc-foundation.io", 18334}
            };
            break;
            
        case TESTNET:
            m_strNetworkID = "test";
            
            m_consensus.nSubsidyHalvingInterval = 210000;
            m_consensus.BIP34Height = 1;
            m_consensus.BIP65Height = 1;
            m_consensus.BIP66Height = 1;
            
            std::fill(m_consensus.powLimit.begin(), m_consensus.powLimit.end(), 0);
            m_consensus.powLimit[28] = 0x00;
            m_consensus.powLimit[29] = 0x1f;
            m_consensus.powLimit[30] = 0xff;
            m_consensus.powLimit[31] = 0xff;
            
            m_consensus.nPowTargetTimespan = 14 * 24 * 60 * 60;
            m_consensus.nPowTargetSpacing = 10 * 60;
            m_consensus.fPowAllowMinDifficultyBlocks = true;
            m_consensus.fPowNoRetargeting = false;
            
            m_consensus.nLWMAWindowSize = 45;
            m_consensus.nProgPoWPeriod = crypto::PROGPOW_PERIOD;
            
            std::fill(m_consensus.hashGenesisBlock.begin(), 
                      m_consensus.hashGenesisBlock.end(), 0);
            
            m_consensus.fProgPowActive = true;
            m_consensus.fMLDSAActive = true;
            
            m_nDefaultPort = 28334;
            m_nRPCPort = 28332;
            
            m_magicBytes = {0x42, 0x48, 0x54, 0x01};
            
            m_base58PrefixPubKey = {0x6F};
            m_base58PrefixScript = {0xC4};
            m_bech32Hrp = "tbhc";
            
            m_vSeeds = {
                {"testnet-seed1.bhc-core.org", 28334},
                {"testnet-seed2.bhc-core.org", 28334}
            };
            break;
            
        case REGTEST:
            m_strNetworkID = "regtest";
            
            m_consensus.nSubsidyHalvingInterval = 150;
            m_consensus.BIP34Height = 1;
            m_consensus.BIP65Height = 1;
            m_consensus.BIP66Height = 1;
            
            std::fill(m_consensus.powLimit.begin(), m_consensus.powLimit.end(), 0);
            m_consensus.powLimit[31] = 0xFF;
            
            m_consensus.nPowTargetTimespan = 14 * 24 * 60 * 60;
            m_consensus.nPowTargetSpacing = 10 * 60;
            m_consensus.fPowAllowMinDifficultyBlocks = true;
            m_consensus.fPowNoRetargeting = true;
            
            m_consensus.nLWMAWindowSize = 45;
            m_consensus.nProgPoWPeriod = crypto::PROGPOW_PERIOD;
            
            std::fill(m_consensus.hashGenesisBlock.begin(), 
                      m_consensus.hashGenesisBlock.end(), 0);
            
            m_consensus.fProgPowActive = true;
            m_consensus.fMLDSAActive = true;
            
            m_nDefaultPort = 38334;
            m_nRPCPort = 38332;
            
            m_magicBytes = {0x42, 0x48, 0x52, 0x01};
            
            m_base58PrefixPubKey = {0x6F};
            m_base58PrefixScript = {0xC4};
            m_bech32Hrp = "rbhc";
            
            m_vSeeds.clear();
            break;
    }
}

const CChainParams& CChainParams::Get() {
    if (!g_current_params) {
        g_current_params = new CChainParams(MAIN);
    }
    return *g_current_params;
}

void CChainParams::SetNetwork(Network network) {
    delete g_current_params;
    g_current_params = new CChainParams(network);
}

}
}
