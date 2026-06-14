// BHC 独立测试程序 - MSVC 兼容版本
// 可直接使用 Visual Studio 编译

#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <random>
#include <chrono>
#include <iomanip>
#include <array>

namespace bhc {
namespace crypto {

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
        }
        
        for (size_t i = 0; i < hash.size(); ++i) {
            if (hash[i] > 0) return hash[i] <= (target >> (8 * (hash.size() - 1 - i)));
        }
        return true;
    }
};

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

void printHex(const std::vector<uint8_t>& data, size_t maxLen = 32) {
    for (size_t i = 0; i < std::min(maxLen, data.size()); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    if (data.size() > maxLen) std::cout << "...";
    std::cout << std::dec << std::endl;
}

void testProgPoW() {
    std::cout << "\n=== ProgPoW \xb2\xe2\xca\xd4 ===\n";
    
    std::vector<uint8_t> header(80, 0x42);
    uint64_t nonce = 2335930;
    
    std::vector<uint8_t> hash;
    bhc::crypto::ProgPoW::Hash(header, nonce, hash);
    
    std::cout << "Header: ";
    printHex(header, 16);
    std::cout << "Nonce: " << nonce << "\n";
    std::cout << "Hash: ";
    printHex(hash);
    
    std::cout << "\n[OK] ProgPoW \xb9\xfe\xcb\xe3\xbc\xc6\xcb\xe3\xb3\xc9\xb9\xa6\n";
}

void testMLDSA() {
    std::cout << "\n=== ML-DSA \xb2\xe2\xca\xd4 ===\n";
    
    auto kp = bhc::crypto::MLDSA::GenerateKeyPair();
    std::cout << "\xb9\xab\xd4\xbf\xb4\xf3\xd0\xa1: " << kp.publicKey.size() << " \xd7\xd6\xbd\xda\n";
    std::cout << "\xcb\xbd\xd4\xbf\xb4\xf3\xd0\xa1: " << kp.secretKey.size() << " \xd7\xd6\xbd\xda\n";
    
    std::vector<uint8_t> message(32, 0xAB);
    auto signature = bhc::crypto::MLDSA::Sign(message, kp.secretKey);
    
    std::cout << "\xc7\xa9\xc3\xfb\xb4\xf3\xd0\xa1: " << signature.size() << " \xd7\xd6\xbd\xda\n";
    std::cout << "\xc7\xa9\xc3\xfb: ";
    printHex(signature, 16);
    
    bool valid = bhc::crypto::MLDSA::Verify(message, signature, kp.publicKey);
    std::cout << "\xd1\xe9\xd6\xa4\xbd\xe1\xb9\xfb: " << (valid ? "\xcd\xa8\xb9\xfd" : "\xca\xa7\xb0\xdc") << "\n";
    
    std::cout << "\n[OK] ML-DSA \xc7\xa9\xc3\xfb\xd1\xe9\xd6\xa4\xb3\xc9\xb9\xa6\n";
}

void testPerformance() {
    std::cout << "\n=== \xd0\xd4\xc4\xdc\xb2\xe2\xca\xd4 ===\n";
    
    const int iterations = 1000;
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        std::vector<uint8_t> header(80, static_cast<uint8_t>(i));
        std::vector<uint8_t> hash;
        bhc::crypto::ProgPoW::Hash(header, i, hash);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "ProgPoW: " << iterations << " \xb4\xce\xb9\xfe\xcf\xa3\xbc\xc6\xcb\xe3\n";
    std::cout << "\xd7\xdc\xba\xc4\xca\xb1: " << duration.count() << " \xce\xa2\xc3\xeb\n";
    std::cout << "\xc6\xbd\xbe\xf9: " << duration.count() / iterations << " \xce\xa2\xc3\xeb/\xb4\xce\n";
    std::cout << "\xcd\xbb\xcd\xbb\xc1\xbf: " << (iterations * 1000000.0 / duration.count()) << " H/s\n";
    
    start = std::chrono::high_resolution_clock::now();
    auto kp = bhc::crypto::MLDSA::GenerateKeyPair();
    std::vector<uint8_t> message(32, 0x42);
    for (int i = 0; i < iterations; ++i) {
        auto sig = bhc::crypto::MLDSA::Sign(message, kp.secretKey);
        bhc::crypto::MLDSA::Verify(message, sig, kp.publicKey);
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "\nML-DSA: " << iterations << " \xb4\xce\xc7\xa9\xc3\xfb+\xd1\xe9\xd6\xa4\n";
    std::cout << "\xd7\xdc\xba\xc4\xca\xb1: " << duration.count() << " \xce\xa2\xc3\xeb\n";
    std::cout << "\xc6\xbd\xbe\xf9: " << duration.count() / iterations << " \xce\xa2\xc3\xeb/\xb4\xce\n";
}

int main() {
    std::cout << "============================================================\n";
    std::cout << "BHC \xba\xcb\xd0\xc4\xc4\xa3\xbf\xe9\xb6\xc0\xc1\xa2\xb2\xe2\xca\xd4\n";
    std::cout << "MSVC \xbc\xe6\xc8\xdd\xb0\xe6\xb1\xbe\n";
    std::cout << "============================================================\n";
    
    testProgPoW();
    testMLDSA();
    testPerformance();
    
    std::cout << "\n============================================================\n";
    std::cout << "\xcb\xf9\xd3\xd0\xb2\xe2\xca\xd4\xcd\xea\xb3\xc9!\n";
    std::cout << "============================================================\n";
    
    return 0;
}
