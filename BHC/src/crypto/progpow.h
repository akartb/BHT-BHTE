// Copyright (c) 2026 BHC Developers
// Distributed under the MIT software license
// ProgPoW: Programmatic Proof-of-Work Algorithm
// Reference: https://github.com/ifdefelse/ProgPOW

#ifndef BHC_CRYPTO_PROGPOW_H
#define BHC_CRYPTO_PROGPOW_H

#include <cstdint>
#include <cstring>
#include <vector>
#include <array>

namespace bhc {
namespace crypto {

constexpr uint32_t PROGPOW_DIGEST_SIZE = 32;
constexpr uint32_t PROGPOW_LANES = 16;
constexpr uint32_t PROGPOW_REGS = 32;
constexpr uint32_t PROGPOW_DAG_LOADS = 4;
constexpr uint32_t PROGPOW_CACHE_BYTES = 16 * 1024;
constexpr uint32_t PROGPOW_DAG_BYTES = 2 * 1024 * 1024 * 1024ULL;
constexpr uint32_t PROGPOW_PERIOD = 12500;
constexpr uint32_t PROGPOW_L1_CACHE = 16 * 1024;

using uint256 = std::array<uint8_t, 32>;

struct ProgPoWContext {
    uint64_t block_number;
    uint64_t nonce;
    uint256 header_hash;
    std::vector<uint8_t> cache;
    std::vector<uint8_t> dag;
    bool initialized;
    
    ProgPoWContext() : block_number(0), nonce(0), initialized(false) {}
};

class CProgPoW {
private:
    uint64_t m_epoch;
    uint64_t m_block_number;
    std::vector<uint8_t> m_cache;
    std::vector<uint8_t> m_dag_light;
    
    static uint32_t fnv1a(uint32_t h, uint32_t d);
    static uint32_t fnv1a(const std::vector<uint32_t>& v, uint32_t d);
    
    void initCache(uint64_t block_number);
    void initDag(uint64_t block_number);
    
    uint32_t calcDatasetItem(const std::vector<uint8_t>& cache, uint32_t index);
    void hashLoop(uint64_t block_number, uint64_t nonce, const uint256& header_hash,
                  std::array<uint32_t, PROGPOW_LANES>& mix);
    
public:
    CProgPoW();
    ~CProgPoW();
    
    void Initialize(uint64_t block_number);
    
    void Hash(const uint256& header_hash, uint64_t nonce, uint256& output);
    
    bool Verify(const uint256& header_hash, uint64_t nonce, const uint256& target);
    
    static uint64_t GetEpoch(uint64_t block_number);
    static uint32_t GetPeriod() { return PROGPOW_PERIOD; }
    
    uint64_t GetCurrentEpoch() const { return m_epoch; }
    uint64_t GetCurrentBlock() const { return m_block_number; }
};

uint256 ProgPoWHash(const uint256& header_hash, uint64_t nonce, uint64_t block_number);

bool CheckProgPoWProof(const uint256& header_hash, uint64_t nonce, 
                       uint64_t block_number, const uint256& target);

}
}

#endif
