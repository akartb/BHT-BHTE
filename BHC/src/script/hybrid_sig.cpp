// Copyright (c) 2026 BHC Developers
// Distributed under the MIT software license
// Hybrid Signature Validation Implementation

#include "script/hybrid_sig.h"
#include "crypto/mldsa.h"
#include "pubkey.h"
#include "hash.h"

#include <algorithm>

namespace bhc {
namespace script {

CHybridSignatureValidator::CHybridSignatureValidator(bool require_quantum_safe,
                                                     crypto::MLDSA_Level level)
    : m_require_quantum_safe(require_quantum_safe)
    , m_mldsa_level(level) {
}

bool CHybridSignatureValidator::verifyECDSA(const std::span<const uint8_t> signature,
                                            const std::span<const uint8_t> pubkey,
                                            const std::span<const uint8_t> message_hash) {
    if (signature.size() < 8 || signature.size() > 72) {
        return false;
    }
    
    if (pubkey.size() != 33 && pubkey.size() != 65) {
        return false;
    }
    
    if (message_hash.size() != 32) {
        return false;
    }
    
    CPubKey pk(pubkey.begin(), pubkey.end());
    if (!pk.IsValid()) {
        return false;
    }
    
    return pk.Verify(uint256(message_hash.data()), signature);
}

bool CHybridSignatureValidator::verifyMLDSA(const std::span<const uint8_t> signature,
                                            const std::span<const uint8_t> pubkey,
                                            const std::span<const uint8_t> message_hash) {
    size_t expected_sig_size = 0;
    size_t expected_pk_size = 0;
    
    switch (m_mldsa_level) {
        case crypto::MLDSA_Level::Level2:
            expected_sig_size = crypto::MLDSA_Signature::SIZE_LEVEL2;
            expected_pk_size = crypto::MLDSA_PublicKey::SIZE_LEVEL2;
            break;
        case crypto::MLDSA_Level::Level3:
            expected_sig_size = crypto::MLDSA_Signature::SIZE_LEVEL3;
            expected_pk_size = crypto::MLDSA_PublicKey::SIZE_LEVEL3;
            break;
        case crypto::MLDSA_Level::Level5:
            expected_sig_size = crypto::MLDSA_Signature::SIZE_LEVEL5;
            expected_pk_size = crypto::MLDSA_PublicKey::SIZE_LEVEL5;
            break;
    }
    
    if (signature.size() != expected_sig_size) {
        return false;
    }
    
    if (pubkey.size() != expected_pk_size) {
        return false;
    }
    
    if (message_hash.size() != 32) {
        return false;
    }
    
    crypto::MLDSA_Signature sig(m_mldsa_level);
    std::copy(signature.begin(), signature.end(), sig.begin());
    
    crypto::MLDSA_PublicKey pk(m_mldsa_level);
    std::copy(pubkey.begin(), pubkey.end(), pk.begin());
    
    return crypto::MLDSA_Verify(message_hash, sig, pk);
}

bool CHybridSignatureValidator::VerifyHybrid(const HybridSignatureData& sig_data,
                                             const std::span<const uint8_t> message_hash) {
    bool ecdsa_valid = false;
    bool mldsa_valid = false;
    
    if (!sig_data.ecdsa_signature.empty() && !sig_data.ecdsa_pubkey.empty()) {
        ecdsa_valid = verifyECDSA(sig_data.ecdsa_signature,
                                  sig_data.ecdsa_pubkey,
                                  message_hash);
    }
    
    if (!sig_data.mldsa_signature.empty() && !sig_data.mldsa_pubkey.empty()) {
        mldsa_valid = verifyMLDSA(sig_data.mldsa_signature,
                                  sig_data.mldsa_pubkey,
                                  message_hash);
    }
    
    if (m_require_quantum_safe) {
        return mldsa_valid;
    }
    
    if (IsPostQuantumWitness(sig_data.sig_version)) {
        return ecdsa_valid && mldsa_valid;
    }
    
    return ecdsa_valid;
}

bool CHybridSignatureValidator::VerifyPostQuantumOnly(const std::span<const uint8_t> signature,
                                                      const std::span<const uint8_t> pubkey,
                                                      const std::span<const uint8_t> message_hash) {
    return verifyMLDSA(signature, pubkey, message_hash);
}

bool CHybridSignatureValidator::VerifyLegacyOnly(const std::span<const uint8_t> signature,
                                                 const std::span<const uint8_t> pubkey,
                                                 const std::span<const uint8_t> message_hash) {
    return verifyECDSA(signature, pubkey, message_hash);
}

bool CHybridSignatureValidator::IsPostQuantumWitness(SigVersion version) {
    return version == SigVersion::WITNESS_V1_BHC || 
           version == SigVersion::WITNESS_V1_PQ;
}

SigVersion CHybridSignatureValidator::DetectSignatureVersion(const std::span<const uint8_t> witness_program) {
    if (witness_program.empty()) {
        return SigVersion::BASE;
    }
    
    uint8_t version = witness_program[0];
    
    switch (version) {
        case 0x00: return SigVersion::WITNESS_V0;
        case 0x01: return SigVersion::WITNESS_V1;
        case 0x42: return SigVersion::WITNESS_V1_BHC;
        case 0x43: return SigVersion::WITNESS_V1_PQ;
        default: return SigVersion::BASE;
    }
}

bool ExecutePostQuantumVerify(const std::vector<uint8_t>& vchSig,
                              const std::vector<uint8_t>& vchPubKey,
                              const std::vector<uint8_t>& vchMessageHash) {
    if (vchSig.size() != crypto::MLDSA_Signature::SIZE_LEVEL3) {
        return false;
    }
    
    if (vchPubKey.size() != crypto::MLDSA_PublicKey::SIZE_LEVEL3) {
        return false;
    }
    
    crypto::MLDSA_Signature sig(crypto::MLDSA_Level::Level3);
    std::copy(vchSig.begin(), vchSig.end(), sig.begin());
    
    crypto::MLDSA_PublicKey pk(crypto::MLDSA_Level::Level3);
    std::copy(vchPubKey.begin(), vchPubKey.end(), pk.begin());
    
    std::span<const uint8_t> msg(vchMessageHash);
    return crypto::MLDSA_Verify(msg, sig, pk);
}

bool EvalCheckSigHybrid(const std::vector<uint8_t>& vchSig,
                        const std::vector<uint8_t>& vchPubKey,
                        const std::vector<uint8_t>& vchMessageHash,
                        SigVersion sigversion) {
    CHybridSignatureValidator validator;
    
    if (CHybridSignatureValidator::IsPostQuantumWitness(sigversion)) {
        if (vchSig.size() > 3000) {
            return ExecutePostQuantumVerify(vchSig, vchPubKey, vchMessageHash);
        }
    }
    
    HybridSignatureData sig_data;
    sig_data.sig_version = sigversion;
    
    if (vchSig.size() <= 72) {
        sig_data.ecdsa_signature = vchSig;
        sig_data.ecdsa_pubkey = vchPubKey;
    } else {
        size_t ecdsa_sig_len = 71;
        size_t ecdsa_pk_len = 33;
        
        if (vchSig.size() >= ecdsa_sig_len + ecdsa_pk_len + crypto::MLDSA_Signature::SIZE_LEVEL3) {
            sig_data.ecdsa_signature.assign(vchSig.begin(), vchSig.begin() + ecdsa_sig_len);
            sig_data.ecdsa_pubkey.assign(vchSig.begin() + ecdsa_sig_len, 
                                         vchSig.begin() + ecdsa_sig_len + ecdsa_pk_len);
            sig_data.mldsa_signature.assign(vchSig.begin() + ecdsa_sig_len + ecdsa_pk_len,
                                            vchSig.begin() + ecdsa_sig_len + ecdsa_pk_len + crypto::MLDSA_Signature::SIZE_LEVEL3);
            sig_data.mldsa_pubkey = vchPubKey;
            sig_data.use_post_quantum = true;
        }
    }
    
    std::span<const uint8_t> msg_hash(vchMessageHash);
    return validator.VerifyHybrid(sig_data, msg_hash);
}

size_t CalculatePQWitnessWeight(size_t signature_size, size_t pubkey_size) {
    constexpr size_t PQ_WITNESS_DISCOUNT = 16;
    
    size_t base_weight = (signature_size + pubkey_size) * 4;
    
    size_t discounted_weight = base_weight / PQ_WITNESS_DISCOUNT;
    
    return std::max(discounted_weight, static_cast<size_t>(200));
}

}
}
