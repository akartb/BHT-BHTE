// Copyright (c) 2026 BHC Developers
// Distributed under the MIT software license
// Hybrid Signature Validation - ECDSA + ML-DSA

#ifndef BHC_SCRIPT_HYBRID_SIG_H
#define BHC_SCRIPT_HYBRID_SIG_H

#include <cstdint>
#include <vector>
#include <span>
#include <crypto/mldsa.h>
#include <type_bridge.h>

namespace bhc {
namespace script {

enum class SigVersion : uint8_t {
    BASE = 0,
    WITNESS_V0 = 1,
    WITNESS_V1 = 2,
    WITNESS_V1_BHC = 0x42,
    WITNESS_V1_PQ = 0x43
};

struct HybridSignatureData {
    std::vector<uint8_t> ecdsa_signature;
    std::vector<uint8_t> mldsa_signature;
    std::vector<uint8_t> ecdsa_pubkey;
    std::vector<uint8_t> mldsa_pubkey;
    bool use_post_quantum;
    SigVersion sig_version;
    
    HybridSignatureData() : use_post_quantum(false), sig_version(SigVersion::BASE) {}
};

class CHybridSignatureValidator {
private:
    static constexpr size_t ECDSA_SIG_SIZE = 71;
    static constexpr size_t ECDSA_PUBKEY_SIZE = 33;
    static constexpr size_t MLDSA_SIG_SIZE_LEVEL3 = 3293;
    static constexpr size_t MLDSA_PUBKEY_SIZE_LEVEL3 = 1952;
    
    bool m_require_quantum_safe;
    crypto::MLDSA_Level m_mldsa_level;
    
    bool verifyECDSA(const std::span<const uint8_t> signature,
                     const std::span<const uint8_t> pubkey,
                     const std::span<const uint8_t> message_hash);
    
    bool verifyMLDSA(const std::span<const uint8_t> signature,
                     const std::span<const uint8_t> pubkey,
                     const std::span<const uint8_t> message_hash);
    
public:
    explicit CHybridSignatureValidator(bool require_quantum_safe = false,
                                        crypto::MLDSA_Level level = crypto::MLDSA_Level::Level3);
    
    bool VerifyHybrid(const HybridSignatureData& sig_data,
                      const std::span<const uint8_t> message_hash);
    
    bool VerifyPostQuantumOnly(const std::span<const uint8_t> signature,
                                const std::span<const uint8_t> pubkey,
                                const std::span<const uint8_t> message_hash);
    
    bool VerifyLegacyOnly(const std::span<const uint8_t> signature,
                          const std::span<const uint8_t> pubkey,
                          const std::span<const uint8_t> message_hash);
    
    static bool IsPostQuantumWitness(SigVersion version);
    static SigVersion DetectSignatureVersion(const std::span<const uint8_t> witness_program);
    
    void SetRequireQuantumSafe(bool require) { m_require_quantum_safe = require; }
    bool GetRequireQuantumSafe() const { return m_require_quantum_safe; }
};

bool ExecutePostQuantumVerify(const std::vector<uint8_t>& vchSig,
                              const std::vector<uint8_t>& vchPubKey,
                              const std::vector<uint8_t>& vchMessageHash);

bool EvalCheckSigHybrid(const std::vector<uint8_t>& vchSig,
                        const std::vector<uint8_t>& vchPubKey,
                        const std::vector<uint8_t>& vchMessageHash,
                        SigVersion sigversion);

size_t CalculatePQWitnessWeight(size_t signature_size, size_t pubkey_size);

inline type_bridge::crypto_uint256 ToBhcCryptoHash(const std::vector<uint8_t>& hash) {
    return type_bridge::ToBhcCrypto(hash);
}

inline std::vector<uint8_t> FromBhcCryptoHash(const type_bridge::crypto_uint256& hash) {
    return type_bridge::FromBhcCrypto(hash);
}

}
}

#endif
