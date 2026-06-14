// Copyright (c) 2026 BHC Developers
// Distributed under the MIT software license
// BHC Chain Parameters - Main Network Configuration

#ifndef BHC_CHAINPARAMS_H
#define BHC_CHAINPARAMS_H

#include <cstdint>
#include <string>
#include <vector>
#include <array>

namespace bhc {
namespace chain {

struct ConsensusParams {
    uint64_t nSubsidyHalvingInterval;
    uint32_t BIP34Height;
    uint32_t BIP65Height;
    uint32_t BIP66Height;
    
    std::array<uint8_t, 32> powLimit;
    int64_t nPowTargetTimespan;
    int64_t nPowTargetSpacing;
    bool fPowAllowMinDifficultyBlocks;
    bool fPowNoRetargeting;
    
    uint32_t nLWMAWindowSize;
    uint64_t nProgPoWPeriod;
    
    std::array<uint8_t, 32> hashGenesisBlock;
    
    bool fProgPowActive;
    bool fMLDSAActive;
};

struct SeedNode {
    std::string host;
    uint16_t port;
};

class CChainParams {
public:
    enum Network {
        MAIN,
        TESTNET,
        REGTEST
    };
    
private:
    Network m_network;
    std::string m_strNetworkID;
    ConsensusParams m_consensus;
    std::vector<SeedNode> m_vSeeds;
    
    uint16_t m_nDefaultPort;
    uint16_t m_nRPCPort;
    
    std::vector<uint8_t> m_base58PrefixPubKey;
    std::vector<uint8_t> m_base58PrefixScript;
    std::string m_bech32Hrp;
    
    std::array<uint8_t, 4> m_magicBytes;
    
public:
    CChainParams(Network network);
    
    const ConsensusParams& GetConsensus() const { return m_consensus; }
    const std::string& NetworkID() const { return m_strNetworkID; }
    Network GetNetwork() const { return m_network; }
    
    uint16_t GetDefaultPort() const { return m_nDefaultPort; }
    uint16_t GetRPCPort() const { return m_nRPCPort; }
    
    const std::vector<SeedNode>& GetSeeds() const { return m_vSeeds; }
    
    const std::vector<uint8_t>& Base58PrefixPubKey() const { return m_base58PrefixPubKey; }
    const std::vector<uint8_t>& Base58PrefixScript() const { return m_base58PrefixScript; }
    const std::string& Bech32HRP() const { return m_bech32Hrp; }
    
    const std::array<uint8_t, 4>& MagicBytes() const { return m_magicBytes; }
    
    bool IsMain() const { return m_network == MAIN; }
    bool IsTestNet() const { return m_network == TESTNET; }
    bool IsRegTest() const { return m_network == REGTEST; }
    
    static const CChainParams& Get();
    static void SetNetwork(Network network);
};

}
}

#endif
