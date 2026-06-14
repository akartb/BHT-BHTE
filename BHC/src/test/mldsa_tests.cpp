// Copyright (c) 2026 BHC Developers
// Distributed under the MIT software license
// ML-DSA Unit Tests

#include <crypto/mldsa.h>
#include <test/util/setup_common.h>
#include <boost/test/unit_test.hpp>
#include <vector>
#include <random>
#include <cstring>

BOOST_FIXTURE_TEST_SUITE(mldsa_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(mldsa_keypair_generation_level3)
{
    bhc::crypto::CMLDSA signer(bhc::crypto::MLDSA_Level::Level3);
    
    BOOST_CHECK(!signer.HasKey());
    
    signer.GenerateKeyPair();
    
    BOOST_CHECK(signer.HasKey());
    
    const auto& pubkey = signer.GetPublicKey();
    const auto& seckey = signer.GetSecretKey();
    
    BOOST_CHECK_EQUAL(pubkey.size(), bhc::crypto::MLDSA_PublicKey::SIZE_LEVEL3);
    BOOST_CHECK_EQUAL(seckey.size(), bhc::crypto::MLDSA_SecretKey::SIZE_LEVEL3);
    
    BOOST_CHECK_NE(pubkey.data()[0], 0);
}

BOOST_AUTO_TEST_CASE(mldsa_sign_verify_basic)
{
    bhc::crypto::CMLDSA signer(bhc::crypto::MLDSA_Level::Level3);
    signer.GenerateKeyPair();
    
    std::vector<uint8_t> message = {0x01, 0x02, 0x03, 0x04, 0x05};
    
    bhc::crypto::MLDSA_Signature signature(bhc::crypto::MLDSA_Level::Level3);
    signer.Sign(message, signature);
    
    BOOST_CHECK_EQUAL(signature.size(), bhc::crypto::MLDSA_Signature::SIZE_LEVEL3);
    
    bool verified = signer.Verify(message, signature);
    BOOST_CHECK(verified);
}

BOOST_AUTO_TEST_CASE(mldsa_sign_verify_hash)
{
    bhc::crypto::CMLDSA signer(bhc::crypto::MLDSA_Level::Level3);
    signer.GenerateKeyPair();
    
    std::vector<uint8_t> hash(32, 0);
    for (int i = 0; i < 32; ++i) {
        hash[i] = static_cast<uint8_t>(i);
    }
    
    bhc::crypto::MLDSA_Signature signature(bhc::crypto::MLDSA_Level::Level3);
    signer.Sign(hash, signature);
    
    bool verified = signer.Verify(hash, signature);
    BOOST_CHECK(verified);
}

BOOST_AUTO_TEST_CASE(mldsa_tampered_signature)
{
    bhc::crypto::CMLDSA signer(bhc::crypto::MLDSA_Level::Level3);
    signer.GenerateKeyPair();
    
    std::vector<uint8_t> message = {0x01, 0x02, 0x03, 0x04, 0x05};
    
    bhc::crypto::MLDSA_Signature signature(bhc::crypto::MLDSA_Level::Level3);
    signer.Sign(message, signature);
    
    signature.data()[0] ^= 0x01;
    
    bool verified = signer.Verify(message, signature);
    BOOST_CHECK(!verified);
}

BOOST_AUTO_TEST_CASE(mldsa_tampered_message)
{
    bhc::crypto::CMLDSA signer(bhc::crypto::MLDSA_Level::Level3);
    signer.GenerateKeyPair();
    
    std::vector<uint8_t> message = {0x01, 0x02, 0x03, 0x04, 0x05};
    
    bhc::crypto::MLDSA_Signature signature(bhc::crypto::MLDSA_Level::Level3);
    signer.Sign(message, signature);
    
    std::vector<uint8_t> tampered_message = {0x01, 0x02, 0x03, 0x04, 0xFF};
    
    bool verified = signer.Verify(tampered_message, signature);
    BOOST_CHECK(!verified);
}

BOOST_AUTO_TEST_CASE(mldsa_wrong_key)
{
    bhc::crypto::CMLDSA signer1(bhc::crypto::MLDSA_Level::Level3);
    bhc::crypto::CMLDSA signer2(bhc::crypto::MLDSA_Level::Level3);
    
    signer1.GenerateKeyPair();
    signer2.GenerateKeyPair();
    
    std::vector<uint8_t> message = {0x01, 0x02, 0x03, 0x04, 0x05};
    
    bhc::crypto::MLDSA_Signature signature(bhc::crypto::MLDSA_Level::Level3);
    signer1.Sign(message, signature);
    
    bool verified = signer2.Verify(message, signature);
    BOOST_CHECK(!verified);
}

BOOST_AUTO_TEST_CASE(mldsa_static_verify)
{
    bhc::crypto::CMLDSA signer(bhc::crypto::MLDSA_Level::Level3);
    signer.GenerateKeyPair();
    
    std::vector<uint8_t> message = {0x01, 0x02, 0x03, 0x04, 0x05};
    
    bhc::crypto::MLDSA_Signature signature(bhc::crypto::MLDSA_Level::Level3);
    signer.Sign(message, signature);
    
    const auto& pubkey = signer.GetPublicKey();
    
    bool verified = bhc::crypto::CMLDSA::Verify(message, signature, pubkey);
    BOOST_CHECK(verified);
}

BOOST_AUTO_TEST_CASE(mldsa_context_sign_verify)
{
    bhc::crypto::CMLDSA signer(bhc::crypto::MLDSA_Level::Level3);
    signer.GenerateKeyPair();
    
    std::vector<uint8_t> message = {0x01, 0x02, 0x03, 0x04, 0x05};
    std::vector<uint8_t> context = {0xB, 0xC};
    
    bhc::crypto::MLDSA_Signature signature(bhc::crypto::MLDSA_Level::Level3);
    signer.Sign(message, signature, context);
    
    bool verified = signer.Verify(message, signature, context);
    BOOST_CHECK(verified);
    
    std::vector<uint8_t> wrong_context = {0xB, 0xD};
    bool wrong_verified = signer.Verify(message, signature, wrong_context);
    BOOST_CHECK(!wrong_verified);
}

BOOST_AUTO_TEST_CASE(mldsa_empty_message)
{
    bhc::crypto::CMLDSA signer(bhc::crypto::MLDSA_Level::Level3);
    signer.GenerateKeyPair();
    
    std::vector<uint8_t> empty_message;
    
    bhc::crypto::MLDSA_Signature signature(bhc::crypto::MLDSA_Level::Level3);
    signer.Sign(empty_message, signature);
    
    bool verified = signer.Verify(empty_message, signature);
    BOOST_CHECK(verified);
}

BOOST_AUTO_TEST_CASE(mldsa_large_message)
{
    bhc::crypto::CMLDSA signer(bhc::crypto::MLDSA_Level::Level3);
    signer.GenerateKeyPair();
    
    std::vector<uint8_t> large_message(10000, 0xAB);
    
    bhc::crypto::MLDSA_Signature signature(bhc::crypto::MLDSA_Level::Level3);
    signer.Sign(large_message, signature);
    
    bool verified = signer.Verify(large_message, signature);
    BOOST_CHECK(verified);
}

BOOST_AUTO_TEST_CASE(mldsa_multiple_signatures)
{
    bhc::crypto::CMLDSA signer(bhc::crypto::MLDSA_Level::Level3);
    signer.GenerateKeyPair();
    
    for (int i = 0; i < 10; ++i) {
        std::vector<uint8_t> message(32, static_cast<uint8_t>(i));
        
        bhc::crypto::MLDSA_Signature signature(bhc::crypto::MLDSA_Level::Level3);
        signer.Sign(message, signature);
        
        bool verified = signer.Verify(message, signature);
        BOOST_CHECK(verified);
    }
}

BOOST_AUTO_TEST_CASE(mldsa_level2_keysize)
{
    bhc::crypto::CMLDSA signer(bhc::crypto::MLDSA_Level::Level2);
    signer.GenerateKeyPair();
    
    BOOST_CHECK_EQUAL(signer.GetPublicKey().size(), 
                       bhc::crypto::MLDSA_PublicKey::SIZE_LEVEL2);
    BOOST_CHECK_EQUAL(signer.GetSecretKey().size(), 
                       bhc::crypto::MLDSA_SecretKey::SIZE_LEVEL2);
}

BOOST_AUTO_TEST_CASE(mldsa_level5_keysize)
{
    bhc::crypto::CMLDSA signer(bhc::crypto::MLDSA_Level::Level5);
    signer.GenerateKeyPair();
    
    BOOST_CHECK_EQUAL(signer.GetPublicKey().size(), 
                       bhc::crypto::MLDSA_PublicKey::SIZE_LEVEL5);
    BOOST_CHECK_EQUAL(signer.GetSecretKey().size(), 
                       bhc::crypto::MLDSA_SecretKey::SIZE_LEVEL5);
}

BOOST_AUTO_TEST_CASE(mldsa_free_functions)
{
    bhc::crypto::CMLDSA signer(bhc::crypto::MLDSA_Level::Level3);
    signer.GenerateKeyPair();
    
    std::vector<uint8_t> message = {0x01, 0x02, 0x03, 0x04, 0x05};
    
    bhc::crypto::MLDSA_Signature signature(bhc::crypto::MLDSA_Level::Level3);
    bhc::crypto::MLDSA_Sign(message, signer.GetSecretKey(), signature);
    
    bool verified = bhc::crypto::MLDSA_Verify(message, signature, signer.GetPublicKey());
    BOOST_CHECK(verified);
}

BOOST_AUTO_TEST_CASE(mldsa_size_functions)
{
    BOOST_CHECK_EQUAL(
        bhc::crypto::CMLDSA::GetSignatureSize(bhc::crypto::MLDSA_Level::Level2),
        bhc::crypto::MLDSA_Signature::SIZE_LEVEL2
    );
    
    BOOST_CHECK_EQUAL(
        bhc::crypto::CMLDSA::GetSignatureSize(bhc::crypto::MLDSA_Level::Level3),
        bhc::crypto::MLDSA_Signature::SIZE_LEVEL3
    );
    
    BOOST_CHECK_EQUAL(
        bhc::crypto::CMLDSA::GetSignatureSize(bhc::crypto::MLDSA_Level::Level5),
        bhc::crypto::MLDSA_Signature::SIZE_LEVEL5
    );
    
    BOOST_CHECK_EQUAL(
        bhc::crypto::CMLDSA::GetPublicKeySize(bhc::crypto::MLDSA_Level::Level3),
        bhc::crypto::MLDSA_PublicKey::SIZE_LEVEL3
    );
    
    BOOST_CHECK_EQUAL(
        bhc::crypto::CMLDSA::GetSecretKeySize(bhc::crypto::MLDSA_Level::Level3),
        bhc::crypto::MLDSA_SecretKey::SIZE_LEVEL3
    );
}

BOOST_AUTO_TEST_SUITE_END()
