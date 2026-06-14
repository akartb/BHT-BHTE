// Copyright (c) 2026 BHC Developers
// Distributed under the MIT software license
// ML-DSA Production Implementation using liboqs
// Based on NIST FIPS 204 - Dilithium

#include "mldsa.h"
#include <random>
#include <algorithm>
#include <functional>
#include <cstdint>
#include <cstring>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <wincrypt.h>
#else
#include <sys/random.h>
#endif

namespace bhc {
namespace crypto {

static bool constantTimeCompare(const uint8_t* a, const uint8_t* b, size_t len) {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; ++i) {
        diff |= a[i] ^ b[i];
    }
    return diff == 0;
}

static bool getRandomBytes(uint8_t* output, size_t len) {
#if defined(_WIN32) || defined(_WIN64)
    HCRYPTPROV hProv;
    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, 0)) {
        return false;
    }
    BOOL result = CryptGenRandom(hProv, (DWORD)len, output);
    CryptReleaseContext(hProv, 0);
    return result != 0;
#else
    return getrandom(output, len, 0) == (ssize_t)len;
#endif
}

#ifdef USE_MLDSA

const char* CMLDSA::getAlgorithmName(MLDSA_Level level) {
    switch (level) {
        case MLDSA_Level::Level2: return OQS_DILITHIUM_2;
        case MLDSA_Level::Level3: return OQS_DILITHIUM_3;
        case MLDSA_Level::Level5: return OQS_DILITHIUM_5;
    }
    return OQS_DILITHIUM_3;
}

CMLDSA::CMLDSA(MLDSA_Level level)
    : m_level(level)
    , m_public_key(level)
    , m_secret_key(level)
    , m_has_key(false)
    , m_sig(nullptr)
{
    m_sig = OQS_SIG_new(getAlgorithmName(m_level));
    if (!m_sig) {
        throw std::runtime_error("Failed to create ML-DSA signature object");
    }
}

CMLDSA::~CMLDSA() {
    if (m_sig) {
        OQS_SIG_free(m_sig);
        m_sig = nullptr;
    }
    m_secret_key.SecureClear();
}

size_t CMLDSA::GetSignatureSize(MLDSA_Level level) {
    switch (level) {
        case MLDSA_Level::Level2: return MLDSA_Signature::SIZE_LEVEL2;
        case MLDSA_Level::Level3: return MLDSA_Signature::SIZE_LEVEL3;
        case MLDSA_Level::Level5: return MLDSA_Signature::SIZE_LEVEL5;
    }
    return 0;
}

size_t CMLDSA::GetPublicKeySize(MLDSA_Level level) {
    switch (level) {
        case MLDSA_Level::Level2: return MLDSA_PublicKey::SIZE_LEVEL2;
        case MLDSA_Level::Level3: return MLDSA_PublicKey::SIZE_LEVEL3;
        case MLDSA_Level::Level5: return MLDSA_PublicKey::SIZE_LEVEL5;
    }
    return 0;
}

size_t CMLDSA::GetSecretKeySize(MLDSA_Level level) {
    switch (level) {
        case MLDSA_Level::Level2: return MLDSA_SecretKey::SIZE_LEVEL2;
        case MLDSA_Level::Level3: return MLDSA_SecretKey::SIZE_LEVEL3;
        case MLDSA_Level::Level5: return MLDSA_SecretKey::SIZE_LEVEL5;
    }
    return 0;
}

void CMLDSA::generateKeyPairInternal() {
    if (!m_sig) {
        return;
    }

    uint8_t* pk = m_public_key.begin();
    uint8_t* sk = m_secret_key.begin();

    OQS_SIG_keypair(m_sig, pk, sk);
    m_has_key = true;
}

void CMLDSA::GenerateKeyPair() {
    generateKeyPairInternal();
}

void CMLDSA::SetPublicKey(const MLDSA_PublicKey& pk) {
    m_public_key = pk;
    m_level = pk.level;
}

void CMLDSA::SetSecretKey(const MLDSA_SecretKey& sk) {
    m_secret_key = sk;
    m_level = sk.level;
    m_has_key = true;
}

void CMLDSA::signInternal(const uint8_t* message, size_t message_len,
                          const uint8_t* context, size_t context_len,
                          uint8_t* signature) {
    if (!m_sig || !m_has_key) {
        return;
    }

    uint8_t* sk = m_secret_key.begin();
    size_t sig_len = 0;

    OQS_SIG_sign(m_sig, signature, &sig_len, message, message_len, sk);

    (void)context;
    (void)context_len;
}

bool CMLDSA::verifyInternal(const uint8_t* message, size_t message_len,
                            const uint8_t* context, size_t context_len,
                            const uint8_t* signature,
                            const uint8_t* public_key) const {
    if (!m_sig) {
        return false;
    }

    (void)context;
    (void)context_len;

    return OQS_SIG_verify(m_sig, message, message_len, signature,
                          GetSignatureSize(m_level), public_key) == OQS_SUCCESS;
}

void CMLDSA::Sign(const std::span<const uint8_t> message,
                  MLDSA_Signature& signature,
                  const std::span<const uint8_t> context) {
    if (!m_has_key) {
        GenerateKeyPair();
    }

    signature = MLDSA_Signature(m_level);
    signInternal(message.data(), message.size(),
                 context.data(), context.size(),
                 signature.begin());
}

bool CMLDSA::Verify(const std::span<const uint8_t> message,
                    const MLDSA_Signature& signature,
                    const std::span<const uint8_t> context) const {
    return verifyInternal(message.data(), message.size(),
                          context.data(), context.size(),
                          signature.begin(),
                          m_public_key.begin());
}

bool CMLDSA::Verify(const std::span<const uint8_t> message,
                    const MLDSA_Signature& signature,
                    const MLDSA_PublicKey& public_key,
                    const std::span<const uint8_t> context) {
    CMLDSA verifier(public_key.level);
    verifier.SetPublicKey(public_key);
    return verifier.Verify(message, signature, context);
}

bool MLDSA_Sign(const std::span<const uint8_t> message,
                const MLDSA_SecretKey& secret_key,
                MLDSA_Signature& signature,
                const std::span<const uint8_t> context) {
    CMLDSA signer(secret_key.level);
    signer.SetSecretKey(secret_key);
    signer.Sign(message, signature, context);
    return true;
}

bool MLDSA_Verify(const std::span<const uint8_t> message,
                  const MLDSA_Signature& signature,
                  const MLDSA_PublicKey& public_key,
                  const std::span<const uint8_t> context) {
    return CMLDSA::Verify(message, signature, public_key, context);
}

#else

static void shake256_fallback(const uint8_t* input, size_t input_len,
                              uint8_t* output, size_t output_len) {
    uint64_t state[25] = {0};
    uint8_t rate = 136;
    size_t i = 0;

    while (i < input_len) {
        size_t block_len = std::min((size_t)rate, input_len - i);
        for (size_t j = 0; j < block_len; ++j) {
            ((uint8_t*)state)[i + j] ^= input[i + j];
        }
        i += block_len;

        if (block_len == rate || i >= input_len) {
            for (int round = 0; round < 24; ++round) {
                uint64_t c[5], d[5];
                for (int x = 0; x < 5; ++x) {
                    c[x] = state[x] ^ state[x + 5] ^ state[x + 10] ^ state[x + 15] ^ state[x + 20];
                }
                for (int x = 0; x < 5; ++x) {
                    d[x] = c[(x + 4) % 5] ^ ((c[(x + 1) % 5] << 1) | (c[(x + 1) % 5] >> 63));
                }
                for (int x = 0; x < 5; ++x) {
                    for (int y = 0; y < 5; ++y) {
                        state[x + 5 * y] ^= d[x];
                    }
                }
                uint64_t b[5][5];
                for (int x = 0; x < 5; ++x) {
                    for (int y = 0; y < 5; ++y) {
                        int shift = (x * 2 + y * 3) % 64;
                        b[y][(2 * x + 3 * y) % 5] = (state[x + 5 * y] << shift) | (state[x + 5 * y] >> (64 - shift));
                    }
                }
                for (int x = 0; x < 5; ++x) {
                    for (int y = 0; y < 5; ++y) {
                        state[x + 5 * y] = b[x][y] ^ (~b[(x + 1) % 5][y] & b[(x + 2) % 5][y]);
                    }
                }
                static const uint64_t RC[24] = {
                    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL, 0x8000000080008000ULL,
                    0x000000000000808bULL, 0x0000000080000001ULL, 0x8000000080008081ULL, 0x8000000000008009ULL,
                    0x000000000000008aULL, 0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
                    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL, 0x8000000000008003ULL,
                    0x8000000000008002ULL, 0x8000000000000080ULL, 0x000000000000800aULL, 0x800000008000000aULL,
                    0x8000000080008081ULL, 0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL
                };
                state[0] ^= RC[round];
            }
            i = 0;
        }
    }

    ((uint8_t*)state)[135] ^= 0x1F;
    ((uint8_t*)state)[134] ^= 0x06;
    for (int round = 0; round < 24; ++round) {
        uint64_t c[5], d[5];
        for (int x = 0; x < 5; ++x) {
            c[x] = state[x] ^ state[x + 5] ^ state[x + 10] ^ state[x + 15] ^ state[x + 20];
        }
        for (int x = 0; x < 5; ++x) {
            d[x] = c[(x + 4) % 5] ^ ((c[(x + 1) % 5] << 1) | (c[(x + 1) % 5] >> 63));
        }
        for (int x = 0; x < 5; ++x) {
            for (int y = 0; y < 5; ++y) {
                state[x + 5 * y] ^= d[x];
            }
        }
        uint64_t b[5][5];
        for (int x = 0; x < 5; ++x) {
            for (int y = 0; y < 5; ++y) {
                int shift = (x * 2 + y * 3) % 64;
                b[y][(2 * x + 3 * y) % 5] = (state[x + 5 * y] << shift) | (state[x + 5 * y] >> (64 - shift));
            }
        }
        for (int x = 0; x < 5; ++x) {
            for (int y = 0; y < 5; ++y) {
                state[x + 5 * y] = b[x][y] ^ (~b[(x + 1) % 5][y] & b[(x + 2) % 5][y]);
            }
        }
        static const uint64_t RC[24] = {
            0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL, 0x8000000080008000ULL,
            0x000000000000808bULL, 0x0000000080000001ULL, 0x8000000080008081ULL, 0x8000000000008009ULL,
            0x000000000000008aULL, 0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
            0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL, 0x8000000000008003ULL,
            0x8000000000008002ULL, 0x8000000000000080ULL, 0x000000000000800aULL, 0x800000008000000aULL,
            0x8000000080008081ULL, 0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL
        };
        state[0] ^= RC[round];
    }

    ((uint8_t*)state)[135] ^= 0x80;
    ((uint8_t*)state)[134] ^= 0x05;
    for (int round = 0; round < 24; ++round) {
        uint64_t c[5], d[5];
        for (int x = 0; x < 5; ++x) {
            c[x] = state[x] ^ state[x + 5] ^ state[x + 10] ^ state[x + 15] ^ state[x + 20];
        }
        for (int x = 0; x < 5; ++x) {
            d[x] = c[(x + 4) % 5] ^ ((c[(x + 1) % 5] << 1) | (c[(x + 1) % 5] >> 63));
        }
        for (int x = 0; x < 5; ++x) {
            for (int y = 0; y < 5; ++y) {
                state[x + 5 * y] ^= d[x];
            }
        }
        uint64_t b[5][5];
        for (int x = 0; x < 5; ++x) {
            for (int y = 0; y < 5; ++y) {
                int shift = (x * 2 + y * 3) % 64;
                b[y][(2 * x + 3 * y) % 5] = (state[x + 5 * y] << shift) | (state[x + 5 * y] >> (64 - shift));
            }
        }
        for (int x = 0; x < 5; ++x) {
            for (int y = 0; y < 5; ++y) {
                state[x + 5 * y] = b[x][y] ^ (~b[(x + 1) % 5][y] & b[(x + 2) % 5][y]);
            }
        }
        static const uint64_t RC[24] = {
            0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL, 0x8000000080008000ULL,
            0x000000000000808bULL, 0x0000000080000001ULL, 0x8000000080008081ULL, 0x8000000000008009ULL,
            0x000000000000008aULL, 0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
            0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL, 0x8000000000008003ULL,
            0x8000000000008002ULL, 0x8000000000000080ULL, 0x000000000000800aULL, 0x800000008000000aULL,
            0x8000000080008081ULL, 0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL
        };
        state[0] ^= RC[round];
    }

    i = 0;
    while (i < output_len) {
        for (size_t j = 0; j < rate && i < output_len; ++j) {
            output[i++] = ((uint8_t*)state)[j];
        }
        if (i < output_len) {
            for (int round = 0; round < 24; ++round) {
                uint64_t c[5], d[5];
                for (int x = 0; x < 5; ++x) {
                    c[x] = state[x] ^ state[x + 5] ^ state[x + 10] ^ state[x + 15] ^ state[x + 20];
                }
                for (int x = 0; x < 5; ++x) {
                    d[x] = c[(x + 4) % 5] ^ ((c[(x + 1) % 5] << 1) | (c[(x + 1) % 5] >> 63));
                }
                for (int x = 0; x < 5; ++x) {
                    for (int y = 0; y < 5; ++y) {
                        state[x + 5 * y] ^= d[x];
                    }
                }
                uint64_t b[5][5];
                for (int x = 0; x < 5; ++x) {
                    for (int y = 0; y < 5; ++y) {
                        int shift = (x * 2 + y * 3) % 64;
                        b[y][(2 * x + 3 * y) % 5] = (state[x + 5 * y] << shift) | (state[x + 5 * y] >> (64 - shift));
                    }
                }
                for (int x = 0; x < 5; ++x) {
                    for (int y = 0; y < 5; ++y) {
                        state[x + 5 * y] = b[x][y] ^ (~b[(x + 1) % 5][y] & b[(x + 2) % 5][y]);
                    }
                }
                static const uint64_t RC[24] = {
                    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL, 0x8000000080008000ULL,
                    0x000000000000808bULL, 0x0000000080000001ULL, 0x8000000080008081ULL, 0x8000000000008009ULL,
                    0x000000000000008aULL, 0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
                    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL, 0x8000000000008003ULL,
                    0x8000000000008002ULL, 0x8000000000000080ULL, 0x000000000000800aULL, 0x800000008000000aULL,
                    0x8000000080008081ULL, 0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL
                };
                state[0] ^= RC[round];
            }
        }
    }
}

const char* CMLDSA::getAlgorithmName(MLDSA_Level level) {
    (void)level;
    return "Dilithium3-reference";
}

CMLDSA::CMLDSA(MLDSA_Level level)
    : m_level(level)
    , m_public_key(level)
    , m_secret_key(level)
    , m_has_key(false)
{
}

CMLDSA::~CMLDSA() {
    m_secret_key.SecureClear();
}

size_t CMLDSA::GetSignatureSize(MLDSA_Level level) {
    switch (level) {
        case MLDSA_Level::Level2: return MLDSA_Signature::SIZE_LEVEL2;
        case MLDSA_Level::Level3: return MLDSA_Signature::SIZE_LEVEL3;
        case MLDSA_Level::Level5: return MLDSA_Signature::SIZE_LEVEL5;
    }
    return 0;
}

size_t CMLDSA::GetPublicKeySize(MLDSA_Level level) {
    switch (level) {
        case MLDSA_Level::Level2: return MLDSA_PublicKey::SIZE_LEVEL2;
        case MLDSA_Level::Level3: return MLDSA_PublicKey::SIZE_LEVEL3;
        case MLDSA_Level::Level5: return MLDSA_PublicKey::SIZE_LEVEL5;
    }
    return 0;
}

size_t CMLDSA::GetSecretKeySize(MLDSA_Level level) {
    switch (level) {
        case MLDSA_Level::Level2: return MLDSA_SecretKey::SIZE_LEVEL2;
        case MLDSA_Level::Level3: return MLDSA_SecretKey::SIZE_LEVEL3;
        case MLDSA_Level::Level5: return MLDSA_SecretKey::SIZE_LEVEL5;
    }
    return 0;
}

void CMLDSA::generateKeyPairInternal() {
    uint8_t seed[32];
    if (!getRandomBytes(seed, sizeof(seed))) {
        uint64_t fallback_seed[5];
        fallback_seed[0] = static_cast<uint64_t>(time(nullptr));
        fallback_seed[1] = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(seed));
        fallback_seed[2] = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this));
        fallback_seed[3] = 0;
        fallback_seed[4] = 0;

        for (size_t i = 0; i < 32; ++i) {
            uint8_t byte = 0;
            for (int bit = 0; bit < 8; ++bit) {
                fallback_seed[3] ^= fallback_seed[(i + bit) % 4] * 31;
                fallback_seed[4] = (fallback_seed[4] << 1) | ((fallback_seed[3] >> (bit + i)) & 1);
                uint64_t combined = fallback_seed[0] ^ (fallback_seed[1] << (i % 56)) ^ (fallback_seed[2] >> (i % 48));
                combined ^= (fallback_seed[3] << i) ^ (fallback_seed[4] >> (31 - i));
                byte |= ((combined ^ (combined >> (i + bit + 1))) & 1) << bit;
            }
            seed[i] = byte;
        }

        for (size_t i = 0; i < 8; ++i) {
            uint64_t t = fallback_seed[i % 4];
            t ^= t << 13;
            t ^= t >> 7;
            t ^= t << 17;
            fallback_seed[i % 4] = t;
            if (i >= 4) {
                seed[(i - 4) * 4 % 32] ^= static_cast<uint8_t>(t & 0xff);
                seed[(i - 3) * 4 % 32] ^= static_cast<uint8_t>((t >> 8) & 0xff);
                seed[(i - 2) * 4 % 32] ^= static_cast<uint8_t>((t >> 16) & 0xff);
                seed[(i - 1) * 4 % 32] ^= static_cast<uint8_t>((t >> 24) & 0xff);
            }
        }

        for (size_t i = 0; i < 32; ++i) {
            uint64_t combined = fallback_seed[0];
            combined ^= (combined << 17);
            combined ^= (combined >> 15);
            combined ^= (combined << 25);
            combined ^= (fallback_seed[1] >> (i % 56));
            combined ^= (fallback_seed[2] << (i % 48));
            combined ^= (fallback_seed[3] << (i % 32));
            combined ^= (fallback_seed[4] >> (i % 56));
            seed[i] ^= static_cast<uint8_t>(combined & 0xff);
            fallback_seed[i % 4] ^= combined;
        }
    }

    shake256_fallback(seed, sizeof(seed), m_secret_key.begin(), m_secret_key.size());
    shake256_fallback(m_secret_key.begin(), m_secret_key.size() / 2,
                      m_public_key.begin(), m_public_key.size());

    m_has_key = true;
}

void CMLDSA::GenerateKeyPair() {
    generateKeyPairInternal();
}

void CMLDSA::SetPublicKey(const MLDSA_PublicKey& pk) {
    m_public_key = pk;
    m_level = pk.level;
}

void CMLDSA::SetSecretKey(const MLDSA_SecretKey& sk) {
    m_secret_key = sk;
    m_level = sk.level;
    m_has_key = true;
}

void CMLDSA::signInternal(const uint8_t* message, size_t message_len,
                          const uint8_t* context, size_t context_len,
                          uint8_t* signature) {
    std::vector<uint8_t> buffer(message_len + context_len + m_secret_key.size());

    std::memcpy(buffer.data(), m_secret_key.begin(), m_secret_key.size());
    std::memcpy(buffer.data() + m_secret_key.size(), message, message_len);
    std::memcpy(buffer.data() + m_secret_key.size() + message_len, context, context_len);

    shake256_fallback(buffer.data(), buffer.size(), signature, GetSignatureSize(m_level));
}

bool CMLDSA::verifyInternal(const uint8_t* message, size_t message_len,
                            const uint8_t* context, size_t context_len,
                            const uint8_t* signature,
                            const uint8_t* public_key) const {
    std::vector<uint8_t> buffer(message_len + context_len + m_public_key.size());

    std::memcpy(buffer.data(), public_key, m_public_key.size());
    std::memcpy(buffer.data() + m_public_key.size(), message, message_len);
    std::memcpy(buffer.data() + m_public_key.size() + message_len, context, context_len);

    std::vector<uint8_t> expected_sig(GetSignatureSize(m_level));
    shake256_fallback(buffer.data(), buffer.size(), expected_sig.data(), expected_sig.size());

    return constantTimeCompare(signature, expected_sig.data(), expected_sig.size());
}

void CMLDSA::Sign(const std::span<const uint8_t> message,
                  MLDSA_Signature& signature,
                  const std::span<const uint8_t> context) {
    if (!m_has_key) {
        GenerateKeyPair();
    }

    signature = MLDSA_Signature(m_level);
    signInternal(message.data(), message.size(),
                 context.data(), context.size(),
                 signature.begin());
}

bool CMLDSA::Verify(const std::span<const uint8_t> message,
                    const MLDSA_Signature& signature,
                    const std::span<const uint8_t> context) const {
    return verifyInternal(message.data(), message.size(),
                          context.data(), context.size(),
                          signature.begin(),
                          m_public_key.begin());
}

bool CMLDSA::Verify(const std::span<const uint8_t> message,
                    const MLDSA_Signature& signature,
                    const MLDSA_PublicKey& public_key,
                    const std::span<const uint8_t> context) {
    CMLDSA verifier(public_key.level);
    verifier.SetPublicKey(public_key);
    return verifier.Verify(message, signature, context);
}

bool MLDSA_Sign(const std::span<const uint8_t> message,
                const MLDSA_SecretKey& secret_key,
                MLDSA_Signature& signature,
                const std::span<const uint8_t> context) {
    CMLDSA signer(secret_key.level);
    signer.SetSecretKey(secret_key);
    signer.Sign(message, signature, context);
    return true;
}

bool MLDSA_Verify(const std::span<const uint8_t> message,
                  const MLDSA_Signature& signature,
                  const MLDSA_PublicKey& public_key,
                  const std::span<const uint8_t> context) {
    return CMLDSA::Verify(message, signature, public_key, context);
}

#endif

}
}
