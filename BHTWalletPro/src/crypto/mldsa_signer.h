// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - ML-DSA Post-Quantum Signature Implementation

#ifndef BHT_WALLET_CRYPTO_MLDSA_H
#define BHT_WALLET_CRYPTO_MLDSA_H

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <openssl/ec.h>
#include <openssl/evp.h>

#ifdef USE_MLDSA
#include <oqs/oqs.h>
#endif

namespace bht {
namespace crypto {

enum class MLDSALevel {
    Level2,
    Level3,
    Level5
};

class CMLDSASigner {
public:
    explicit CMLDSASigner(MLDSALevel level = MLDSALevel::Level3);
    ~CMLDSASigner();

    bool GenerateKeyPair();
    bool SetKeyPair(const std::vector<uint8_t>& private_key, const std::vector<uint8_t>& public_key);

    std::vector<uint8_t> Sign(const std::vector<uint8_t>& message);
    bool SignInPlace(std::vector<uint8_t>& data);

    bool Verify(const std::vector<uint8_t>& message, const std::vector<uint8_t>& signature);

    std::vector<uint8_t> GetPublicKey() const { return m_public_key; }
    std::vector<uint8_t> GetPrivateKey() const { return m_private_key; }

    size_t GetSignatureSize() const;
    size_t GetPublicKeySize() const;
    size_t GetPrivateKeySize() const;

    MLDSALevel GetLevel() const { return m_level; }
    bool HasKey() const { return m_has_key; }

    void ClearKeys();

    static size_t GetSignatureSizeForLevel(MLDSALevel level);
    static size_t GetPublicKeySizeForLevel(MLDSALevel level);
    static size_t GetPrivateKeySizeForLevel(MLDSALevel level);

    const char* GetAlgorithmName() const;

private:
    MLDSALevel m_level;
    bool m_has_key{false};

    std::vector<uint8_t> m_public_key;
    std::vector<uint8_t> m_private_key;

#ifdef USE_MLDSA
    OQS_SIG* m_sig{nullptr};
#endif

    const char* getAlgorithmName() const;
};

class CMLDSAHybridSigner {
public:
    CMLDSAHybridSigner(MLDSALevel level = MLDSALevel::Level3);

    bool GenerateKeyPair();
    bool SetKeyPair(
        const std::vector<uint8_t>& ecdsa_private,
        const std::vector<uint8_t>& ecdsa_public,
        const std::vector<uint8_t>& mldsa_private,
        const std::vector<uint8_t>& mldsa_public
    );

    std::vector<uint8_t> Sign(const std::vector<uint8_t>& message, bool hybrid = true);
    bool Verify(const std::vector<uint8_t>& message, const std::vector<uint8_t>& signature, bool hybrid = true);

    bool HasECDSAKey() const { return m_has_ecdsa_key; }
    bool HasMLDSAKey() const { return m_has_mldsa_key; }

    std::vector<uint8_t> GetECDSAKey() const { return m_ecdsa_private; }
    std::vector<uint8_t> GetECDSACompactKey() const { return m_ecdsa_public; }
    std::vector<uint8_t> GetMLDSAKey() const { return m_mldsa_private; }
    std::vector<uint8_t> GetMLDSAPublic() const { return m_mldsa_public; }

private:
    std::unique_ptr<CMLDSASigner> m_mldsa_signer;

    bool m_has_ecdsa_key{false};
    bool m_has_mldsa_key{false};

    std::vector<uint8_t> m_ecdsa_private;
    std::vector<uint8_t> m_ecdsa_public;
    std::vector<uint8_t> m_mldsa_private;
    std::vector<uint8_t> m_mldsa_public;
};

}
}

#endif
