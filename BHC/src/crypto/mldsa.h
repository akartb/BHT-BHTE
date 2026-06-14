// Copyright (c) 2026 BHC Developers
// Distributed under the MIT software license
// ML-DSA (Module-Lattice-Based Digital Signature Standard)
// Based on NIST FIPS 204 - Dilithium
// Production implementation using liboqs: https://github.com/open-quantum-safe/liboqs

#ifndef BHC_CRYPTO_MLDSA_H
#define BHC_CRYPTO_MLDSA_H

#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <span>
#include <stdexcept>

#ifdef USE_MLDSA
#include <oqs/oqs.h>
#endif

namespace bhc {
namespace crypto {

enum class MLDSA_Level : uint8_t {
    Level2 = 2,
    Level3 = 3,
    Level5 = 5
};

struct MLDSA_PublicKey {
    static constexpr size_t SIZE_LEVEL2 = 1312;
    static constexpr size_t SIZE_LEVEL3 = 1952;
    static constexpr size_t SIZE_LEVEL5 = 2592;

    std::vector<uint8_t> data;
    MLDSA_Level level;

    MLDSA_PublicKey(MLDSA_Level lvl = MLDSA_Level::Level3) : level(lvl) {
        switch (level) {
            case MLDSA_Level::Level2: data.resize(SIZE_LEVEL2); break;
            case MLDSA_Level::Level3: data.resize(SIZE_LEVEL3); break;
            case MLDSA_Level::Level5: data.resize(SIZE_LEVEL5); break;
        }
    }

    size_t size() const { return data.size(); }
    const uint8_t* begin() const { return data.data(); }
    uint8_t* begin() { return data.data(); }
    const uint8_t* end() const { return data.data() + data.size(); }
    uint8_t* end() { return data.data() + data.size(); }
};

struct MLDSA_SecretKey {
    static constexpr size_t SIZE_LEVEL2 = 2560;
    static constexpr size_t SIZE_LEVEL3 = 4032;
    static constexpr size_t SIZE_LEVEL5 = 4896;

    std::vector<uint8_t> data;
    MLDSA_Level level;

    MLDSA_SecretKey(MLDSA_Level lvl = MLDSA_Level::Level3) : level(lvl) {
        switch (level) {
            case MLDSA_Level::Level2: data.resize(SIZE_LEVEL2); break;
            case MLDSA_Level::Level3: data.resize(SIZE_LEVEL3); break;
            case MLDSA_Level::Level5: data.resize(SIZE_LEVEL5); break;
        }
    }

    size_t size() const { return data.size(); }
    const uint8_t* begin() const { return data.data(); }
    uint8_t* begin() { return data.data(); }
    const uint8_t* end() const { return data.data() + data.size(); }
    uint8_t* end() { return data.data() + data.size(); }

    void SecureClear() {
        std::memset(data.data(), 0, data.size());
    }
};

struct MLDSA_Signature {
    static constexpr size_t SIZE_LEVEL2 = 2420;
    static constexpr size_t SIZE_LEVEL3 = 3293;
    static constexpr size_t SIZE_LEVEL5 = 4595;

    std::vector<uint8_t> data;
    MLDSA_Level level;

    MLDSA_Signature(MLDSA_Level lvl = MLDSA_Level::Level3) : level(lvl) {
        switch (level) {
            case MLDSA_Level::Level2: data.resize(SIZE_LEVEL2); break;
            case MLDSA_Level::Level3: data.resize(SIZE_LEVEL3); break;
            case MLDSA_Level::Level5: data.resize(SIZE_LEVEL5); break;
        }
    }

    size_t size() const { return data.size(); }
    const uint8_t* begin() const { return data.data(); }
    uint8_t* begin() { return data.data(); }
    const uint8_t* end() const { return data.data() + data.size(); }
    uint8_t* end() { return data.data() + data.size(); }
};

class CMLDSA {
private:
    MLDSA_Level m_level;
    MLDSA_PublicKey m_public_key;
    MLDSA_SecretKey m_secret_key;
    bool m_has_key;

#ifdef USE_MLDSA
    OQS_SIG* m_sig;
#endif

    void generateKeyPairInternal();
    void signInternal(const uint8_t* message, size_t message_len,
                      const uint8_t* context, size_t context_len,
                      uint8_t* signature);
    bool verifyInternal(const uint8_t* message, size_t message_len,
                        const uint8_t* context, size_t context_len,
                        const uint8_t* signature,
                        const uint8_t* public_key) const;

    static const char* getAlgorithmName(MLDSA_Level level);

public:
    explicit CMLDSA(MLDSA_Level level = MLDSA_Level::Level3);
    ~CMLDSA();

    void GenerateKeyPair();

    void SetPublicKey(const MLDSA_PublicKey& pk);
    void SetSecretKey(const MLDSA_SecretKey& sk);

    const MLDSA_PublicKey& GetPublicKey() const { return m_public_key; }
    const MLDSA_SecretKey& GetSecretKey() const { return m_secret_key; }

    void Sign(const std::span<const uint8_t> message,
              MLDSA_Signature& signature,
              const std::span<const uint8_t> context = {});

    bool Verify(const std::span<const uint8_t> message,
                const MLDSA_Signature& signature,
                const std::span<const uint8_t> context = {}) const;

    static bool Verify(const std::span<const uint8_t> message,
                       const MLDSA_Signature& signature,
                       const MLDSA_PublicKey& public_key,
                       const std::span<const uint8_t> context = {});

    MLDSA_Level GetLevel() const { return m_level; }
    bool HasKey() const { return m_has_key; }

    static size_t GetSignatureSize(MLDSA_Level level);
    static size_t GetPublicKeySize(MLDSA_Level level);
    static size_t GetSecretKeySize(MLDSA_Level level);
};

bool MLDSA_Sign(const std::span<const uint8_t> message,
                const MLDSA_SecretKey& secret_key,
                MLDSA_Signature& signature,
                const std::span<const uint8_t> context = {});

bool MLDSA_Verify(const std::span<const uint8_t> message,
                  const MLDSA_Signature& signature,
                  const MLDSA_PublicKey& public_key,
                  const std::span<const uint8_t> context = {});

}
}

#endif
