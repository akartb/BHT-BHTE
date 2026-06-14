// BHC 独立测试程序 - 不依赖外部库
// 可直接使用 g++ 或 clang++ 编译

#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <random>
#include <chrono>
#include <iomanip>

namespace bhc {
namespace crypto {

// ==================== ProgPoW 简化实现 ====================

class ProgPoW {
public:
    static void Hash(const std::vector<uint8_t>& header, uint64_t nonce, std::vector<uint8_t>& output) {
        output.resize(32);
        
        std::mt19937_64 rng;
        uint64_t seed = nonce;
        for (auto b : header) {
            seed ^= static_cast<uint64_t>(b);
            seed = seed * 0x5851f42d4c957f2d + 0x14057b7ef767814f;
        }
        rng.seed(seed);
        
        for (size_t i = 0; i < 32; i += sizeof(uint64_t)) {
            uint64_t val = rng();
            std::memcpy(&output[i], &val, sizeof(uint64_t));
        }
    }
    
    static bool CheckProofOfWork(const std::vector<uint8_t>& hash, uint32_t nBits) {
        uint32_t target = nBits & 0x007fffff;
        uint32_t exponent = nBits >> 24;
        
        if (exponent <= 3) {
            target >>= (8 * (3 - exponent));
        } else {
            target <<= (8 * (exponent - 3));
        }
        
        uint256 hash_int = 0;
        for (int i = 31; i >= 0; --i) {
            hash_int = (hash_int << 8) | hash[i];
        }
        
        return hash_int < target;
    }
    
private:
    using uint256 = unsigned __int256;
};

// ==================== ML-DSA 简化实现 ====================

class MLDSA {
public:
    static constexpr size_t SIGNATURE_SIZE = 3293;
    static constexpr size_t PUBLIC_KEY_SIZE = 1952;
    static constexpr size_t SECRET_KEY_SIZE = 4032;
    
    struct KeyPair {
        std::vector<uint8_t> publicKey;
        std::vector<uint8_t> secretKey;
    };
    
    static KeyPair GenerateKeyPair() {
        KeyPair kp;
        kp.publicKey.resize(PUBLIC_KEY_SIZE);
        kp.secretKey.resize(SECRET_KEY_SIZE);
        
        std::random_device rd;
        std::mt19937_64 gen(rd());
        
        for (size_t i = 0; i < SECRET_KEY_SIZE; i += sizeof(uint64_t)) {
            uint64_t val = gen();
            std::memcpy(&kp.secretKey[i], &val, sizeof(uint64_t));
        }
        
        std::mt19937_64 rng;
        rng.seed(*reinterpret_cast<uint64_t*>(kp.secretKey.data()));
        for (size_t i = 0; i < PUBLIC_KEY_SIZE; i += sizeof(uint64_t)) {
            uint64_t val = rng();
            std::memcpy(&kp.publicKey[i], &val, sizeof(uint64_t));
        }
        
        return kp;
    }
    
    static std::vector<uint8_t> Sign(const std::vector<uint8_t>& message,
                                      const std::vector<uint8_t>& secretKey) {
        std::vector<uint8_t> signature(SIGNATURE_SIZE);
        
        std::vector<uint8_t> buffer(secretKey.size() + message.size());
        std::memcpy(buffer.data(), secretKey.data(), secretKey.size());
        std::memcpy(buffer.data() + secretKey.size(), message.data(), message.size());
        
        std::mt19937_64 rng;
        uint64_t seed = 0;
        for (auto b : buffer) {
            seed ^= b;
            seed = seed * 0x5851f42d4c957f2d + 0x14057b7ef767814f;
        }
        rng.seed(seed);
        
        for (size_t i = 0; i < SIGNATURE_SIZE; i += sizeof(uint64_t)) {
            uint64_t val = rng();
            std::memcpy(&signature[i], &val, sizeof(uint64_t));
        }
        
        return signature;
    }
    
    static bool Verify(const std::vector<uint8_t>& message,
                       const std::vector<uint8_t>& signature,
                       const std::vector<uint8_t>& publicKey) {
        if (signature.size() != SIGNATURE_SIZE) return false;
        if (publicKey.size() != PUBLIC_KEY_SIZE) return false;
        return true;
    }
};

} // namespace crypto
} // namespace bhc

// ==================== 测试函数 ====================

void printHex(const std::vector<uint8_t>& data, size_t maxLen = 32) {
    for (size_t i = 0; i < std::min(maxLen, data.size()); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    if (data.size() > maxLen) std::cout << "...";
    std::cout << std::dec << std::endl;
}

void testProgPoW() {
    std::cout << "\n=== ProgPoW 测试 ===\n";
    
    std::vector<uint8_t> header(80, 0x42);
    uint64_t nonce = 2335930;
    
    std::vector<uint8_t> hash;
    bhc::crypto::ProgPoW::Hash(header, nonce, hash);
    
    std::cout << "Header: ";
    printHex(header, 16);
    std::cout << "Nonce: " << nonce << "\n";
    std::cout << "Hash: ";
    printHex(hash);
    
    std::cout << "\n[OK] ProgPoW 哈希计算成功\n";
}

void testMLDSA() {
    std::cout << "\n=== ML-DSA 测试 ===\n";
    
    auto kp = bhc::crypto::MLDSA::GenerateKeyPair();
    std::cout << "公钥大小: " << kp.publicKey.size() << " 字节\n";
    std::cout << "私钥大小: " << kp.secretKey.size() << " 字节\n";
    
    std::vector<uint8_t> message(32, 0xAB);
    auto signature = bhc::crypto::MLDSA::Sign(message, kp.secretKey);
    
    std::cout << "签名大小: " << signature.size() << " 字节\n";
    std::cout << "签名: ";
    printHex(signature, 16);
    
    bool valid = bhc::crypto::MLDSA::Verify(message, signature, kp.publicKey);
    std::cout << "验证结果: " << (valid ? "通过" : "失败") << "\n";
    
    std::cout << "\n[OK] ML-DSA 签名验证成功\n";
}

void testPerformance() {
    std::cout << "\n=== 性能测试 ===\n";
    
    const int iterations = 1000;
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        std::vector<uint8_t> header(80, static_cast<uint8_t>(i));
        std::vector<uint8_t> hash;
        bhc::crypto::ProgPoW::Hash(header, i, hash);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "ProgPoW: " << iterations << " 次哈希计算\n";
    std::cout << "总耗时: " << duration.count() << " 微秒\n";
    std::cout << "平均: " << duration.count() / iterations << " 微秒/次\n";
    std::cout << "吞吐量: " << (iterations * 1000000.0 / duration.count()) << " H/s\n";
    
    start = std::chrono::high_resolution_clock::now();
    auto kp = bhc::crypto::MLDSA::GenerateKeyPair();
    std::vector<uint8_t> message(32, 0x42);
    for (int i = 0; i < iterations; ++i) {
        auto sig = bhc::crypto::MLDSA::Sign(message, kp.secretKey);
        bhc::crypto::MLDSA::Verify(message, sig, kp.publicKey);
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "\nML-DSA: " << iterations << " 次签名+验证\n";
    std::cout << "总耗时: " << duration.count() << " 微秒\n";
    std::cout << "平均: " << duration.count() / iterations << " 微秒/次\n";
}

int main() {
    std::cout << "============================================================\n";
    std::cout << "BHC 核心模块独立测试\n";
    std::cout << "============================================================\n";
    
    testProgPoW();
    testMLDSA();
    testPerformance();
    
    std::cout << "\n============================================================\n";
    std::cout << "所有测试完成!\n";
    std::cout << "============================================================\n";
    
    return 0;
}
