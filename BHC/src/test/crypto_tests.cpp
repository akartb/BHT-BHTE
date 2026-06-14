// Copyright (c) 2026 BHC Developers
// Distributed under the MIT software license
// ML-DSA Unit Tests

#include <crypto/mldsa.h>
#include <test/util/setup_common.h>
#include <boost/test/unit_test.hpp>
#include <vector>
#include <cstdint>
#include <cstring>

namespace bhc {
namespace test {

BOOST_FIXTURE_TEST_SUITE(crypto_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(mldsa_keypair_generation)
{
    crypto::CMLDSA signer(crypto::MLDSA_Level::Level3);
    
    BOOST_CHECK(!signer.HasKey());
    
    signer.GenerateKeyPair();
    
    BOOST_CHECK(signer.HasKey());
    
    const auto& pubkey = signer.GetPublicKey();
    const auto& seckey = signer.GetSecretKey();
    
    BOOST_CHECK_EQUAL(pubkey.size(), crypto::MLDSA_PublicKey::SIZE_LEVEL3);
    BOOST_CHECK_EQUAL(seckey.size(), crypto::MLDSA_SecretKey::SIZE_LEVEL3);
    
    bool all_zero = true;
    for (auto b : pubkey.data) {
        if (b != 0) {
            all_zero = false;
            break;
        }
    }
    BOOST_CHECK(!all_zero);
}

BOOST_AUTO_TEST_CASE(mldsa_sign_verify_basic)
{
    crypto::CMLDSA signer(crypto::MLDSA_Level::Level3);
    signer.GenerateKeyPair();

    std::vector<uint8_t> message(32, 0x01);
    for (size_t i = 0; i < message.size(); ++i) {
        message[i] = static_cast<uint8_t>(i);
    }
    
    crypto::MLDSA_Signature signature(crypto::MLDSA_Level::Level3);
    
    std::span<const uint8_t> msg_span(message);
    signer.Sign(msg_span, signature);
    
    BOOST_CHECK_EQUAL(signature.size(), crypto::MLDSA_Signature::SIZE_LEVEL3);
    
    bool verified = signer.Verify(msg_span, signature);
    BOOST_CHECK(verified);
}

BOOST_AUTO_TEST_CASE(mldsa_verify_wrong_signature)
{
    crypto::CMLDSA signer(crypto::MLDSA_Level::Level3);
    signer.GenerateKeyPair();
    
    std::vector<uint8_t> message(32, 0x42);
    crypto::MLDSA_Signature signature(crypto::MLDSA_Level::Level3);
    
    std::span<const uint8_t> msg_span(message);
    signer.Sign(msg_span, signature);
    
    signature.data[0] ^= 0xFF;
    
    bool verified = signer.Verify(msg_span, signature);
    BOOST_CHECK(!verified);
}

BOOST_AUTO_TEST_CASE(mldsa_verify_wrong_message)
{
    crypto::CMLDSA signer(crypto::MLDSA_Level::Level3);
    signer.GenerateKeyPair();
    
    std::vector<uint8_t> message1(32, 0x01);
    std::vector<uint8_t> message2(32, 0x02);
    
    crypto::MLDSA_Signature signature(crypto::MLDSA_Level::Level3);
    
    std::span<const uint8_t> msg_span(message1);
    signer.Sign(msg_span, signature);
    
    std::span<const uint8_t> wrong_msg_span(message2);
    bool verified = signer.Verify(wrong_msg_span, signature);
    BOOST_CHECK(!verified);
}

BOOST_AUTO_TEST_CASE(mldsa_verify_wrong_key)
{
    crypto::CMLDSA signer1(crypto::MLDSA_Level::Level3);
    crypto::CMLDSA signer2(crypto::MLDSA_Level::Level3);
    
    signer1.GenerateKeyPair();
    signer2.GenerateKeyPair();
    
    std::vector<uint8_t> message(32, 0x01);
    crypto::MLDSA_Signature signature(crypto::MLDSA_Level::Level3);
    
    std::span<const uint8_t> msg_span(message);
    signer1.Sign(msg_span, signature);
    
    bool verified = signer2.Verify(msg_span, signature);
    BOOST_CHECK(!verified);
}

BOOST_AUTO_TEST_CASE(mldsa_static_verify)
{
    crypto::CMLDSA signer(crypto::MLDSA_Level::Level3);
    signer.GenerateKeyPair();
    
    std::vector<uint8_t> message(32, 0xAB);
    for (size_t i = 0; i < message.size(); ++i) {
        message[i] = static_cast<uint8_t>(i * 7 + 13);
    }
    
    crypto::MLDSA_Signature signature(crypto::MLDSA_Level::Level3);
    
    std::span<const uint8_t> msg_span(message);
    signer.Sign(msg_span, signature);
    
    const auto& pubkey = signer.GetPublicKey();
    
    bool verified = crypto::MLDSA_Verify(msg_span, signature, pubkey);
    BOOST_CHECK(verified);
}

BOOST_AUTO_TEST_CASE(mldsa_level2_keypair)
{
    crypto::CMLDSA signer(crypto::MLDSA_Level::Level2);
    signer.GenerateKeyPair();
    
    BOOST_CHECK(signer.HasKey());
    BOOST_CHECK_EQUAL(signer.GetPublicKey().size(), crypto::MLDSA_PublicKey::SIZE_LEVEL2);
    BOOST_CHECK_EQUAL(signer.GetSecretKey().size(), crypto::MLDSA_SecretKey::SIZE_LEVEL2);
    
    std::vector<uint8_t> message(32, 0x00);
    crypto::MLDSA_Signature signature(crypto::MLDSA_Level::Level2);
    
    std::span<const uint8_t> msg_span(message);
    signer.Sign(msg_span, signature);
    
    BOOST_CHECK_EQUAL(signature.size(), crypto::MLDSA_Signature::SIZE_LEVEL2);
    BOOST_CHECK(signer.Verify(msg_span, signature));
}

BOOST_AUTO_TEST_CASE(mldsa_level5_keypair)
{
    crypto::CMLDSA signer(crypto::MLDSA_Level::Level5);
    signer.GenerateKeyPair();
    
    BOOST_CHECK(signer.HasKey());
    BOOST_CHECK_EQUAL(signer.GetPublicKey().size(), crypto::MLDSA_PublicKey::SIZE_LEVEL5);
    BOOST_CHECK_EQUAL(signer.GetSecretKey().size(), crypto::MLDSA_SecretKey::SIZE_LEVEL5);
    
    std::vector<uint8_t> message(32, 0x00);
    crypto::MLDSA_Signature signature(crypto::MLDSA_Level::Level5);
    
    std::span<const uint8_t> msg_span(message);
    signer.Sign(msg_span, signature);
    
    BOOST_CHECK_EQUAL(signature.size(), crypto::MLDSA_Signature::SIZE_LEVEL5);
    BOOST_CHECK(signer.Verify(msg_span, signature));
}

BOOST_AUTO_TEST_CASE(mldsa_empty_message)
{
    crypto::CMLDSA signer(crypto::MLDSA_Level::Level3);
    signer.GenerateKeyPair();
    
    std::vector<uint8_t> empty_message;
    crypto::MLDSA_Signature signature(crypto::MLDSA_Level::Level3);
    
    std::span<const uint8_t> msg_span(empty_message);
    signer.Sign(msg_span, signature);
    
    BOOST_CHECK(signer.Verify(msg_span, signature));
}

BOOST_AUTO_TEST_CASE(mldsa_large_message)
{
    crypto::CMLDSA signer(crypto::MLDSA_Level::Level3);
    signer.GenerateKeyPair();
    
    std::vector<uint8_t> large_message(1024, 0x55);
    for (size_t i = 0; i < large_message.size(); ++i) {
        large_message[i] = static_cast<uint8_t>(i % 256);
    }
    
    crypto::MLDSA_Signature signature(crypto::MLDSA_Level::Level3);
    
    std::span<const uint8_t> msg_span(large_message);
    signer.Sign(msg_span, signature);
    
    BOOST_CHECK(signer.Verify(msg_span, signature));
}

BOOST_AUTO_TEST_CASE(mldsa_context_signing)
{
    crypto::CMLDSA signer(crypto::MLDSA_Level::Level3);
    signer.GenerateKeyPair();
    
    std::vector<uint8_t> message(32, 0x01);
    std::vector<uint8_t> context = {0x42, 0x48, 0x43};
    
    crypto::MLDSA_Signature signature(crypto::MLDSA_Level::Level3);
    
    std::span<const uint8_t> msg_span(message);
    std::span<const uint8_t> ctx_span(context);
    signer.Sign(msg_span, signature, ctx_span);
    
    BOOST_CHECK(signer.Verify(msg_span, signature, ctx_span));
    
    std::vector<uint8_t> wrong_context = {0x00, 0x00, 0x00};
    std::span<const uint8_t> wrong_ctx_span(wrong_context);
    BOOST_CHECK(!signer.Verify(msg_span, signature, wrong_ctx_span));
}

BOOST_AUTO_TEST_CASE(mldsa_multiple_signatures)
{
    crypto::CMLDSA signer(crypto::MLDSA_Level::Level3);
    signer.GenerateKeyPair();
    
    const size_t num_signatures = 10;
    std::vector<crypto::MLDSA_Signature> signatures;
    std::vector<std::vector<uint8_t>> messages;
    
    for (size_t i = 0; i < num_signatures; ++i) {
        std::vector<uint8_t> msg(32, static_cast<uint8_t>(i));
        messages.push_back(msg);
        
        crypto::MLDSA_Signature sig(crypto::MLDSA_Level::Level3);
        std::span<const uint8_t> msg_span(msg);
        signer.Sign(msg_span, sig);
        signatures.push_back(sig);
    }
    
    for (size_t i = 0; i < num_signatures; ++i) {
        std::span<const uint8_t> msg_span(messages[i]);
        BOOST_CHECK(signer.Verify(msg_span, signatures[i]));
    }
}

BOOST_AUTO_TEST_SUITE_END()

}
}
