// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Network Configuration

#ifndef BHT_WALLET_QT_NETWORKCONFIG_H
#define BHT_WALLET_QT_NETWORKCONFIG_H

#include <string>
#include <cstdint>

namespace bht {
namespace config {

enum class NetworkType {
    MainNet,
    TestNet,
    Signet,
    RegTest
};

struct NetworkParams {
    NetworkType network;
    const char* name;
    const char* dataDir;
    uint16_t defaultPort;
    uint16_t defaultRpcPort;
    uint16_t defaultWsPort;
    const char* genesisBlock;
    const char* genesisHash;
    int64_t genesisTime;
    uint32_t bip44CoinType;
    const char* bech32Prefix;
    const char* bech32Separator;
    bool requireStandard;
    bool mineBlocksOnDemand;
    const char* dnsSeeds[16];
    const char* fixedSeeds[16];
    int64_t targetSpacing;
    int64_t targetTimespan;
    uint32_t powLimitBits;
    int64_t difficultyAdjustmentInterval;
};

class CChainParams
{
public:
    static const NetworkParams& MainNet();
    static const NetworkParams& TestNet();
    static const NetworkParams& Signet();
    static const NetworkParams& RegTest();

    static const NetworkParams& GetParams(NetworkType type);
    static NetworkType GetNetworkType();
    static void SetNetworkType(NetworkType type);

    static std::string GetDataDir();
    static std::string GetNetworkName();
    static uint16_t GetDefaultPort();
    static uint16_t GetDefaultRpcPort();
    static uint32_t GetBip44CoinType();
    static std::string GetBech32Prefix();
    static bool IsTestNetwork();

private:
    static NetworkType s_current_network;
};

inline bool IsMainNet() { return CChainParams::GetNetworkType() == NetworkType::MainNet; }
inline bool IsTestNet() { return CChainParams::GetNetworkType() == NetworkType::TestNet; }
inline bool IsSignet() { return CChainParams::GetNetworkType() == NetworkType::Signet; }
inline bool IsRegTest() { return CChainParams::GetNetworkType() == NetworkType::RegTest; }
inline bool IsTestChain() { return !IsMainNet(); }

}
}

#endif
