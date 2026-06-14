// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Network Configuration Implementation

#include "networkconfig.h"
#include <cstring>

namespace bht {
namespace config {

NetworkType CChainParams::s_current_network = NetworkType::MainNet;

static const NetworkParams mainNetParams = {
    NetworkType::MainNet,
    "main",
    "BHT",
    8333,
    8332,
    8334,
    "0100000000000000000000000000000000000000000000000000000000000000000000003ba3edfd7a7b12b27ac72c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4a29ab5f49ffff001d1dac2b7c",
    "000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f",
    1231006505,
    0,
    "bc",
    "1",
    true,
    false,
    {
        "seed.bht.org",
        "seed2.bht.org",
        "dnsseed.bht.org",
        nullptr
    },
    {
        nullptr
    },
    600,
    1209600,
    0x1d00ffff,
    2016
};

static const NetworkParams testNetParams = {
    NetworkType::TestNet,
    "test",
    "BHT_testnet",
    18333,
    18332,
    18334,
    "0100000000000000000000000000000000000000000000000000000000000000000000003ba3edfd7a7b12b27ac72c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4adae5494dffff001d1aa4ae18",
    "000000000933ea01ad0ee984209779baaec3ced90fa3f408719526f8d77f4943",
    1296688602,
    1,
    "tb",
    "1",
    true,
    false,
    {
        "testnet-seed.bht.org",
        "testnet-seed2.bht.org",
        nullptr
    },
    {
        nullptr
    },
    600,
    1209600,
    0x1d00ffff,
    2016
};

static const NetworkParams signetParams = {
    NetworkType::Signet,
    "signet",
    "BHT_signet",
    38333,
    38332,
    38334,
    "0100000000000000000000000000000000000000000000000000000000000000000000003ba3edfd7a7b12b27ac72c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4a8f59174ffff001d1dac2b7c",
    "00000008819873e925046c1fd611d2f4b6b3a3d4e1e1e1e1e1e1e1e1e1e1e1e1e",
    1598918400,
    1,
    "tb",
    "1",
    true,
    false,
    {
        "signet-seed.bht.org",
        nullptr
    },
    {
        nullptr
    },
    600,
    1209600,
    0x1d00ffff,
    2016
};

static const NetworkParams regTestParams = {
    NetworkType::RegTest,
    "regtest",
    "BHT_regtest",
    18444,
    18443,
    18445,
    "0100000000000000000000000000000000000000000000000000000000000000000000003ba3edfd7a7b12b27ac72c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4adae5494dffff7f2002000000",
    "0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206",
    1296688602,
    1,
    "bcrt",
    "1",
    false,
    true,
    {
        nullptr
    },
    {
        nullptr
    },
    600,
    1209600,
    0x207fffff,
    2016
};

const NetworkParams& CChainParams::MainNet()
{
    return mainNetParams;
}

const NetworkParams& CChainParams::TestNet()
{
    return testNetParams;
}

const NetworkParams& CChainParams::Signet()
{
    return signetParams;
}

const NetworkParams& CChainParams::RegTest()
{
    return regTestParams;
}

const NetworkParams& CChainParams::GetParams(NetworkType type)
{
    switch (type) {
    case NetworkType::MainNet:
        return MainNet();
    case NetworkType::TestNet:
        return TestNet();
    case NetworkType::Signet:
        return Signet();
    case NetworkType::RegTest:
        return RegTest();
    default:
        return MainNet();
    }
}

NetworkType CChainParams::GetNetworkType()
{
    return s_current_network;
}

void CChainParams::SetNetworkType(NetworkType type)
{
    s_current_network = type;
}

std::string CChainParams::GetDataDir()
{
    return GetParams(s_current_network).dataDir;
}

std::string CChainParams::GetNetworkName()
{
    return GetParams(s_current_network).name;
}

uint16_t CChainParams::GetDefaultPort()
{
    return GetParams(s_current_network).defaultPort;
}

uint16_t CChainParams::GetDefaultRpcPort()
{
    return GetParams(s_current_network).defaultRpcPort;
}

uint32_t CChainParams::GetBip44CoinType()
{
    return GetParams(s_current_network).bip44CoinType;
}

std::string CChainParams::GetBech32Prefix()
{
    return GetParams(s_current_network).bech32Prefix;
}

bool CChainParams::IsTestNetwork()
{
    return s_current_network != NetworkType::MainNet;
}

}
}
