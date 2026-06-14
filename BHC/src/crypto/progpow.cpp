// Copyright (c) 2026 BHC Developers
// Distributed under the MIT software license
// ProgPoW Implementation

#include "progpow.h"
#include <algorithm>
#include <random>
#include <limits>
#include <cstring>

namespace bhc {
namespace crypto {

static const uint32_t FNV_PRIME = 0x01000193;

static inline uint32_t rotl32(uint32_t v, unsigned int c) {
    return (v << c) | (v >> (32 - c));
}

static inline uint32_t rotr32(uint32_t v, unsigned int c) {
    return (v >> c) | (v << (32 - c));
}

static inline uint32_t clz32(uint32_t v) {
    return v ? __builtin_clz(v) : 32;
}

static inline uint32_t popcount32(uint32_t v) {
    return __builtin_popcount(v);
}

class SHA256 {
public:
    static void hash(const uint8_t* input, size_t input_len, uint8_t output[32]) {
        uint32_t h[8] = {
            0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
            0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
        };

        static const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
            0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
            0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
            0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
            0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
            0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
            0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
            0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
            0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
        };

        size_t padded_len = ((input_len + 8) / 64 + 1) * 64;
        std::vector<uint8_t> padded(padded_len, 0);
        std::memcpy(padded.data(), input, input_len);
        padded[input_len] = 0x80;

        uint64_t bit_len = input_len * 8;
        for (int i = 0; i < 8; ++i) {
            padded[padded_len - 8 + i] = (bit_len >> (56 - i * 8)) & 0xff;
        }

        for (size_t chunk = 0; chunk < padded_len / 64; ++chunk) {
            uint32_t w[64];

            for (int i = 0; i < 16; ++i) {
                w[i] = (uint32_t(padded[chunk * 64 + i * 4]) << 24) |
                       (uint32_t(padded[chunk * 64 + i * 4 + 1]) << 16) |
                       (uint32_t(padded[chunk * 64 + i * 4 + 2]) << 8) |
                       uint32_t(padded[chunk * 64 + i * 4 + 3]);
            }

            for (int i = 16; i < 64; ++i) {
                uint32_t s0 = rotr32(w[i-15], 7) ^ rotr32(w[i-15], 18) ^ (w[i-15] >> 3);
                uint32_t s1 = rotr32(w[i-2], 17) ^ rotr32(w[i-2], 19) ^ (w[i-2] >> 10);
                w[i] = w[i-16] + s0 + w[i-7] + s1;
            }

            uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
            uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

            for (int i = 0; i < 64; ++i) {
                uint32_t S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
                uint32_t ch = (e & f) ^ (~e & g);
                uint32_t temp1 = hh + S1 + ch + k[i] + w[i];
                uint32_t S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
                uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
                uint32_t temp2 = S0 + maj;

                hh = g;
                g = f;
                f = e;
                e = d + temp1;
                d = c;
                c = b;
                b = a;
                a = temp1 + temp2;
            }

            h[0] += a; h[1] += b; h[2] += c; h[3] += d;
            h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
        }

        for (int i = 0; i < 8; ++i) {
            output[i * 4] = (h[i] >> 24) & 0xff;
            output[i * 4 + 1] = (h[i] >> 16) & 0xff;
            output[i * 4 + 2] = (h[i] >> 8) & 0xff;
            output[i * 4 + 3] = h[i] & 0xff;
        }
    }
};

CProgPoW::CProgPoW() 
    : m_epoch(0), m_block_number(0) {
}

CProgPoW::~CProgPoW() {
}

uint32_t CProgPoW::fnv1a(uint32_t h, uint32_t d) {
    return (h ^ d) * FNV_PRIME;
}

uint32_t CProgPoW::fnv1a(const std::vector<uint32_t>& v, uint32_t d) {
    uint32_t h = d;
    for (uint32_t u : v) {
        h = fnv1a(h, u);
    }
    return h;
}

uint64_t CProgPoW::GetEpoch(uint64_t block_number) {
    return block_number / PROGPOW_PERIOD;
}

void CProgPoW::initCache(uint64_t block_number) {
    uint64_t epoch = GetEpoch(block_number);
    
    if (m_epoch == epoch && !m_cache.empty()) {
        return;
    }
    
    m_epoch = epoch;
    m_block_number = block_number;
    
    uint64_t cache_size = PROGPOW_CACHE_BYTES;
    m_cache.resize(cache_size);
    
    std::mt19937_64 rng(0);
    for (size_t i = 0; i < cache_size / sizeof(uint64_t); ++i) {
        uint64_t val = rng();
        std::memcpy(&m_cache[i * sizeof(uint64_t)], &val, sizeof(uint64_t));
    }
    
    for (uint64_t i = 0; i < cache_size / sizeof(uint32_t); ++i) {
        uint32_t* cache_words = reinterpret_cast<uint32_t*>(m_cache.data());
        uint32_t prev = cache_words[(i - 1 + cache_size / sizeof(uint32_t)) % (cache_size / sizeof(uint32_t))];
        cache_words[i] = fnv1a(cache_words[i], prev);
    }
}

void CProgPoW::initDag(uint64_t block_number) {
    initCache(block_number);
    
    uint64_t dag_size = PROGPOW_DAG_BYTES;
    m_dag_light.resize(dag_size / 64);
    
    for (uint64_t i = 0; i < dag_size / 64; ++i) {
        uint32_t item = calcDatasetItem(m_cache, static_cast<uint32_t>(i));
        std::memcpy(&m_dag_light[i * sizeof(uint32_t)], &item, sizeof(uint32_t));
    }
}

uint32_t CProgPoW::calcDatasetItem(const std::vector<uint8_t>& cache, uint32_t index) {
    uint64_t cache_size = cache.size();
    uint32_t* cache_words = reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(cache.data()));
    uint64_t num_cache_words = cache_size / sizeof(uint32_t);
    
    uint32_t mix = index;
    
    for (int i = 0; i < 256; ++i) {
        uint32_t idx = fnv1a(mix, i) % static_cast<uint32_t>(num_cache_words);
        mix = fnv1a(mix, cache_words[idx]);
    }
    
    return mix;
}

void CProgPoW::hashLoop(uint64_t block_number, uint64_t nonce, const uint256& header_hash,
                        std::array<uint32_t, PROGPOW_LANES>& mix) {
    uint32_t seed = static_cast<uint32_t>(nonce & 0xFFFFFFFF);
    seed = fnv1a(seed, static_cast<uint32_t>(nonce >> 32));
    
    for (size_t i = 0; i < header_hash.size(); i += sizeof(uint32_t)) {
        uint32_t h;
        std::memcpy(&h, &header_hash[i], sizeof(uint32_t));
        seed = fnv1a(seed, h);
    }
    
    for (uint32_t lane = 0; lane < PROGPOW_LANES; ++lane) {
        mix[lane] = fnv1a(seed, lane);
    }
    
    uint64_t epoch = GetEpoch(block_number);
    uint32_t prog_seed = static_cast<uint32_t>(epoch);
    
    std::mt19937 rng(prog_seed);
    
    for (uint32_t loop = 0; loop < 64; ++loop) {
        uint32_t r = rng();
        
        uint32_t lane = r % PROGPOW_LANES;
        uint32_t dst_reg = (r >> 8) % PROGPOW_REGS;
        uint32_t src_reg = (r >> 16) % PROGPOW_REGS;
        
        uint32_t op = (r >> 24) % 16;
        
        uint32_t result;
        switch (op) {
            case 0:  result = mix[lane] + mix[(lane + 1) % PROGPOW_LANES]; break;
            case 1:  result = mix[lane] * mix[(lane + 1) % PROGPOW_LANES]; break;
            case 2:  result = mix[lane] ^ mix[(lane + 1) % PROGPOW_LANES]; break;
            case 3:  result = rotl32(mix[lane], (r >> 28) % 32); break;
            case 4:  result = rotr32(mix[lane], (r >> 28) % 32); break;
            case 5:  result = clz32(mix[lane]); break;
            case 6:  result = popcount32(mix[lane]); break;
            case 7:  result = mix[lane] & mix[(lane + 1) % PROGPOW_LANES]; break;
            case 8:  result = mix[lane] | mix[(lane + 1) % PROGPOW_LANES]; break;
            case 9:  result = ~mix[lane]; break;
            case 10: result = mix[lane] - mix[(lane + 1) % PROGPOW_LANES]; break;
            case 11: result = mix[lane] << ((r >> 28) % 32); break;
            case 12: result = mix[lane] >> ((r >> 28) % 32); break;
            case 13: result = fnv1a(mix[lane], mix[(lane + 1) % PROGPOW_LANES]); break;
            case 14: result = mix[lane] ^ rotl32(mix[(lane + 1) % PROGPOW_LANES], 7); break;
            default: result = mix[lane]; break;
        }
        
        mix[lane] = fnv1a(mix[lane], result);
    }
}

void CProgPoW::Initialize(uint64_t block_number) {
    initCache(block_number);
    m_block_number = block_number;
}

void CProgPoW::Hash(const uint256& header_hash, uint64_t nonce, uint256& output) {
    std::array<uint32_t, PROGPOW_LANES> mix;

    hashLoop(m_block_number, nonce, header_hash, mix);

    uint32_t final_hash = 0x811c9dc5;
    for (uint32_t lane = 0; lane < PROGPOW_LANES; ++lane) {
        final_hash = fnv1a(final_hash, mix[lane]);
    }

    std::array<uint8_t, 64> hash_input;
    std::memset(hash_input.data(), 0, 64);
    std::memcpy(hash_input.data(), &final_hash, sizeof(uint32_t));
    std::memcpy(hash_input.data() + 4, &nonce, sizeof(uint64_t));
    std::memcpy(hash_input.data() + 12, header_hash.data(), 32);

    uint8_t sha256_output[32];
    SHA256::hash(hash_input.data(), hash_input.size(), sha256_output);

    for (size_t i = 0; i < 8; ++i) {
        output[i] = (uint32_t(sha256_output[i * 4]) << 24) |
                     (uint32_t(sha256_output[i * 4 + 1]) << 16) |
                     (uint32_t(sha256_output[i * 4 + 2]) << 8) |
                     uint32_t(sha256_output[i * 4 + 3]);
    }
}

bool CProgPoW::Verify(const uint256& header_hash, uint64_t nonce, const uint256& target) {
    uint256 output;
    Hash(header_hash, nonce, output);
    
    for (size_t i = 0; i < output.size(); ++i) {
        if (output[i] < target[i]) return true;
        if (output[i] > target[i]) return false;
    }
    return true;
}

uint256 ProgPoWHash(const uint256& header_hash, uint64_t nonce, uint64_t block_number) {
    CProgPoW progpow;
    progpow.Initialize(block_number);
    
    uint256 output;
    progpow.Hash(header_hash, nonce, output);
    return output;
}

bool CheckProgPoWProof(const uint256& header_hash, uint64_t nonce, 
                       uint64_t block_number, const uint256& target) {
    uint256 hash = ProgPoWHash(header_hash, nonce, block_number);
    
    for (size_t i = 0; i < hash.size(); ++i) {
        if (hash[i] < target[i]) return true;
        if (hash[i] > target[i]) return false;
    }
    return true;
}

}
}
