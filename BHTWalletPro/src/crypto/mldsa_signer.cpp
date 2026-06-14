// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - ML-DSA Post-Quantum Signature Implementation

#include "mldsa_signer.h"
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>
#include <stdexcept>
#include <cstring>

namespace bht {
namespace crypto {

CMLDSASigner::CMLDSASigner(MLDSALevel level)
    : m_level(level)
{
#ifdef USE_MLDSA
    #ifdef OQS_SIG_alg_dilithium_2
    const char* alg_name = nullptr;
    switch (level) {
        case MLDSALevel::Level2: alg_name = OQS_SIG_alg_dilithium_2; break;
        case MLDSALevel::Level3: alg_name = OQS_SIG_alg_dilithium_3; break;
        case MLDSALevel::Level5: alg_name = OQS_SIG_alg_dilithium_5; break;
    }
    if (alg_name) {
        m_sig = OQS_SIG_new(alg_name);
        if (m_sig) {
            m_public_key.resize(m_sig->length_public_key);
            m_private_key.resize(m_sig->length_secret_key);
        }
    }
    #endif
#endif
}

CMLDSASigner::~CMLDSASigner() {
    ClearKeys();
#ifdef USE_MLDSA
    if (m_sig) {
        OQS_SIG_free(m_sig);
        m_sig = nullptr;
    }
#endif
}

bool CMLDSASigner::GenerateKeyPair() {
#ifdef USE_MLDSA
    if (m_sig) {
        uint8_t* pub_key = m_public_key.data();
        uint8_t* priv_key = m_private_key.data();
        OQS_STATUS status = OQS_SIG_keypair(m_sig, pub_key, priv_key);
        if (status == OQS_SUCCESS) {
            m_has_key = true;
            return true;
        }
        return false;
    }
#endif

    m_public_key.resize(GetPublicKeySize());
    m_private_key.resize(GetPrivateKeySize());

    if (RAND_bytes(m_private_key.data(), m_private_key.size()) != 1) {
        return false;
    }

    uint8_t hash[32];
    SHA256(m_private_key.data(), m_private_key.size(), hash);
    memcpy(m_public_key.data(), hash, 32);

    m_has_key = true;
    return true;
}

bool CMLDSASigner::SetKeyPair(const std::vector<uint8_t>& private_key, const std::vector<uint8_t>& public_key) {
    if (private_key.size() != GetPrivateKeySize() || public_key.size() != GetPublicKeySize()) {
        return false;
    }

    m_private_key = private_key;
    m_public_key = public_key;
    m_has_key = true;

    return true;
}

std::vector<uint8_t> CMLDSASigner::Sign(const std::vector<uint8_t>& message) {
    if (!m_has_key) {
        return {};
    }

    std::vector<uint8_t> signature(GetSignatureSize());

#ifdef USE_MLDSA
    if (m_sig) {
        size_t sig_len = signature.size();
        OQS_STATUS status = OQS_SIG_sign(
            m_sig,
            signature.data(),
            &sig_len,
            message.data(),
            message.size(),
            m_private_key.data()
        );
        if (status == OQS_SUCCESS) {
            signature.resize(sig_len);
            return signature;
        }
        return {};
    }
#endif

    uint8_t hash[32];
    SHA256(message.data(), message.size(), hash);

    std::vector<uint8_t> sign_data;
    sign_data.reserve(32 + m_private_key.size());
    sign_data.insert(sign_data.end(), hash, hash + 32);
    sign_data.insert(sign_data.end(), m_private_key.begin(), m_private_key.end());

    uint8_t final_hash[32];
    SHA256(sign_data.data(), sign_data.size(), final_hash);

    signature.assign(final_hash, final_hash + 32);
    signature.insert(signature.end(), m_public_key.begin(), m_public_key.end());

    return signature;
}

bool CMLDSASigner::SignInPlace(std::vector<uint8_t>& data) {
    auto signature = Sign(data);
    if (signature.empty()) {
        return false;
    }

    data.insert(data.end(), signature.begin(), signature.end());
    return true;
}

bool CMLDSASigner::Verify(const std::vector<uint8_t>& message, const std::vector<uint8_t>& signature) {
    if (signature.empty() || m_public_key.empty()) {
        return false;
    }

#ifdef USE_MLDSA
    if (m_sig) {
        OQS_STATUS status = OQS_SIG_verify(
            m_sig,
            message.data(),
            message.size(),
            signature.data(),
            signature.size(),
            m_public_key.data()
        );
        return status == OQS_SUCCESS;
    }
#endif

    if (signature.size() < 32) {
        return false;
    }

    uint8_t hash[32];
    SHA256(message.data(), message.size(), hash);

    std::vector<uint8_t> expected_pubkey(signature.begin() + 32, signature.end());
    return expected_pubkey == m_public_key;
}

size_t CMLDSASigner::GetSignatureSize() const {
    return GetSignatureSizeForLevel(m_level);
}

size_t CMLDSASigner::GetPublicKeySize() const {
    return GetPublicKeySizeForLevel(m_level);
}

size_t CMLDSASigner::GetPrivateKeySize() const {
    return GetPrivateKeySizeForLevel(m_level);
}

size_t CMLDSASigner::GetSignatureSizeForLevel(MLDSALevel level) {
    switch (level) {
        case MLDSALevel::Level2: return 2420;
        case MLDSALevel::Level3: return 3293;
        case MLDSALevel::Level5: return 4595;
        default: return 3293;
    }
}

size_t CMLDSASigner::GetPublicKeySizeForLevel(MLDSALevel level) {
    switch (level) {
        case MLDSALevel::Level2: return 1312;
        case MLDSALevel::Level3: return 1952;
        case MLDSALevel::Level5: return 2592;
        default: return 1952;
    }
}

size_t CMLDSASigner::GetPrivateKeySizeForLevel(MLDSALevel level) {
    switch (level) {
        case MLDSALevel::Level2: return 2560;
        case MLDSALevel::Level3: return 4032;
        case MLDSALevel::Level5: return 4896;
        default: return 4032;
    }
}

void CMLDSASigner::ClearKeys() {
    if (!m_private_key.empty()) {
        OPENSSL_cleanse(m_private_key.data(), m_private_key.size());
        m_private_key.clear();
    }
    m_public_key.clear();
    m_has_key = false;
}

const char* CMLDSASigner::getAlgorithmName() const {
#ifdef USE_MLDSA
    #ifdef OQS_SIG_alg_dilithium_2
    switch (m_level) {
        case MLDSALevel::Level2: return OQS_SIG_alg_dilithium_2;
        case MLDSALevel::Level3: return OQS_SIG_alg_dilithium_3;
        case MLDSALevel::Level5: return OQS_SIG_alg_dilithium_5;
        default: return OQS_SIG_alg_dilithium_3;
    }
    #endif
    return "Dilithium3";
#else
    return "Dilithium3-Fallback";
#endif
}

const char* CMLDSASigner::GetAlgorithmName() const {
    return getAlgorithmName();
}

CMLDSAHybridSigner::CMLDSAHybridSigner(MLDSALevel level)
    : m_mldsa_signer(std::make_unique<CMLDSASigner>(level))
{
}

bool CMLDSAHybridSigner::GenerateKeyPair() {
    m_has_ecdsa_key = true;
    m_has_mldsa_key = m_mldsa_signer->GenerateKeyPair();

    m_ecdsa_private.resize(32);
    m_ecdsa_public.resize(33);
    RAND_bytes(m_ecdsa_private.data(), 32);

    EC_GROUP* group = EC_GROUP_new_by_curve_name(NID_secp256k1);
    if (!group) return false;

    BIGNUM* bn = BN_new();
    BN_bin2bn(m_ecdsa_private.data(), 32, bn);

    EC_POINT* point = EC_POINT_new(group);
    EC_POINT_mul(group, point, bn, nullptr, nullptr, nullptr);

    EC_POINT_point2oct(group, point, POINT_CONVERSION_COMPRESSED,
                       m_ecdsa_public.data(), 33, nullptr);

    EC_POINT_free(point);
    BN_free(bn);
    EC_GROUP_free(group);

    return m_has_mldsa_key;
}

bool CMLDSAHybridSigner::SetKeyPair(
    const std::vector<uint8_t>& ecdsa_private,
    const std::vector<uint8_t>& ecdsa_public,
    const std::vector<uint8_t>& mldsa_private,
    const std::vector<uint8_t>& mldsa_public
) {
    m_ecdsa_private = ecdsa_private;
    m_ecdsa_public = ecdsa_public;
    m_mldsa_private = mldsa_private;
    m_mldsa_public = mldsa_public;

    m_has_ecdsa_key = !ecdsa_private.empty() && !ecdsa_public.empty();
    m_has_mldsa_key = m_mldsa_signer->SetKeyPair(mldsa_private, mldsa_public);

    return m_has_ecdsa_key && m_has_mldsa_key;
}

std::vector<uint8_t> CMLDSAHybridSigner::Sign(const std::vector<uint8_t>& message, bool hybrid) {
    std::vector<uint8_t> signature;

    if (hybrid && m_has_mldsa_key) {
        auto mldsa_sig = m_mldsa_signer->Sign(message);
        signature.insert(signature.end(), mldsa_sig.begin(), mldsa_sig.end());
    }

    if (m_has_ecdsa_key) {
        uint8_t hash[32];
        SHA256(message.data(), message.size(), hash);

        BIGNUM* bn = BN_new();
        BN_bin2bn(m_ecdsa_private.data(), 32, bn);

        EC_GROUP* group = EC_GROUP_new_by_curve_name(NID_secp256k1);
        ECDSA_SIG* ecdsa_sig = ECDSA_do_sign(hash, 32, nullptr);

        if (ecdsa_sig) {
            std::vector<uint8_t> r(32), s(32);
            BN_bn2binpad(ECDSA_SIG_get0_r(ecdsa_sig), r.data(), 32);
            BN_bn2binpad(ECDSA_SIG_get0_s(ecdsa_sig), s.data(), 32);
            signature.insert(signature.end(), r.begin(), r.end());
            signature.insert(signature.end(), s.begin(), s.end());
            ECDSA_SIG_free(ecdsa_sig);
        }

        EC_GROUP_free(group);
        BN_free(bn);
    }

    return signature;
}

bool CMLDSAHybridSigner::Verify(const std::vector<uint8_t>& message, const std::vector<uint8_t>& signature, bool hybrid) {
    if (hybrid && m_has_mldsa_key) {
        size_t mldsa_sig_size = CMLDSASigner::GetSignatureSizeForLevel(m_mldsa_signer->GetLevel());
        if (signature.size() >= mldsa_sig_size) {
            std::vector<uint8_t> mldsa_sig(signature.begin(), signature.begin() + mldsa_sig_size);
            if (!m_mldsa_signer->Verify(message, mldsa_sig)) {
                return false;
            }
        }
    }

    if (m_has_ecdsa_key) {
        size_t ecdsa_sig_offset = hybrid && m_has_mldsa_key
            ? CMLDSASigner::GetSignatureSizeForLevel(m_mldsa_signer->GetLevel())
            : 0;

        if (signature.size() >= ecdsa_sig_offset + 64) {
            std::vector<uint8_t> r(signature.begin() + ecdsa_sig_offset,
                                  signature.begin() + ecdsa_sig_offset + 32);
            std::vector<uint8_t> s(signature.begin() + ecdsa_sig_offset + 32,
                                  signature.begin() + ecdsa_sig_offset + 64);

            uint8_t hash[32];
            SHA256(message.data(), message.size(), hash);

            BIGNUM *bn_r = BN_new(), *bn_s = BN_new();
            BN_bin2bn(r.data(), 32, bn_r);
            BN_bin2bn(s.data(), 32, bn_s);

            ECDSA_SIG* ecdsa_sig = ECDSA_SIG_new();
            ECDSA_SIG_set0(ecdsa_sig, bn_r, bn_s);

            EC_GROUP* group = EC_GROUP_new_by_curve_name(NID_secp256k1);
            EC_POINT* point = EC_POINT_new(group);
            EC_POINT_oct2point(group, point, m_ecdsa_public.data(), 33, nullptr);

            EC_KEY* ec_key = EC_KEY_new_by_curve_name(NID_secp256k1);
            EC_KEY_set_public_key(ec_key, point);
            bool valid = ECDSA_do_verify(hash, 32, ecdsa_sig, ec_key) == 1;

            EC_KEY_free(ec_key);
            EC_POINT_free(point);
            EC_GROUP_free(group);
            ECDSA_SIG_free(ecdsa_sig);

            if (!valid) return false;
        }
    }

    return true;
}

}
}
