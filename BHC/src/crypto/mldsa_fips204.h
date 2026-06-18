// Copyright (c) 2026 BHC Developers
// Distributed under the MIT software license
//
// Header-only NIST FIPS 204 ML-DSA (Module-Lattice-Based Digital Signature)
// for the non-liboqs build path. Ported from the verified Go implementation in
// BHTE/mldsa and the validated standalone self-test
// (BHC/tools/mldsa_fips204_selftest.cpp). Provides keygen / sign / verify over
// serialized keys and signatures so it can back the CMLDSA fallback directly.
//
// Parameter mapping: MLDSA_Level::Level2 -> ML-DSA-44, Level3 -> ML-DSA-65,
// Level5 -> ML-DSA-87. Assumes a little-endian host (true on x86/x64).

#ifndef BHC_CRYPTO_MLDSA_FIPS204_H
#define BHC_CRYPTO_MLDSA_FIPS204_H

#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <random>
#include <initializer_list>

namespace bhc {
namespace crypto {
namespace fips204 {

// ---- Keccak / SHAKE --------------------------------------------------------

inline uint64_t rotl64(uint64_t x, int n) { return (x << n) | (x >> (64 - n)); }

inline void keccakf(uint64_t st[25]) {
    static const uint64_t RC[24] = {
        0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
        0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
        0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
        0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
        0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
        0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
        0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
        0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL};
    static const int rho[24] = {1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 2, 14,
                                27, 41, 56, 8, 25, 43, 62, 18, 39, 61, 20, 44};
    static const int pi[24] = {10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4,
                               15, 23, 19, 13, 12, 2, 20, 14, 22, 9, 6, 1};
    for (int round = 0; round < 24; round++) {
        uint64_t bc[5];
        for (int i = 0; i < 5; i++)
            bc[i] = st[i] ^ st[i + 5] ^ st[i + 10] ^ st[i + 15] ^ st[i + 20];
        for (int i = 0; i < 5; i++) {
            uint64_t t = bc[(i + 4) % 5] ^ rotl64(bc[(i + 1) % 5], 1);
            for (int j = 0; j < 25; j += 5) st[j + i] ^= t;
        }
        uint64_t t = st[1];
        for (int i = 0; i < 24; i++) {
            int j = pi[i];
            uint64_t tmp = st[j];
            st[j] = rotl64(t, rho[i]);
            t = tmp;
        }
        for (int j = 0; j < 25; j += 5) {
            uint64_t a[5];
            for (int i = 0; i < 5; i++) a[i] = st[j + i];
            for (int i = 0; i < 5; i++) st[j + i] = a[i] ^ ((~a[(i + 1) % 5]) & a[(i + 2) % 5]);
        }
        st[0] ^= RC[round];
    }
}

// Shake: absorb-all-then-squeeze XOF (full input known up front).
class Shake {
public:
    Shake(int rateBytes, const std::vector<uint8_t>& in) : rate(rateBytes) {
        std::memset(st, 0, sizeof(st));
        absorb(in.data(), in.size());
    }
    void read(uint8_t* out, size_t outlen) {
        uint8_t* sb = reinterpret_cast<uint8_t*>(st);
        for (size_t i = 0; i < outlen; i++) {
            if (squeezePos == rate) { keccakf(st); squeezePos = 0; }
            out[i] = sb[squeezePos++];
        }
    }
    uint8_t readByte() { uint8_t b; read(&b, 1); return b; }

private:
    uint64_t st[25];
    int rate;
    int squeezePos = 0;
    void absorb(const uint8_t* in, size_t inlen) {
        uint8_t* sb = reinterpret_cast<uint8_t*>(st);
        size_t blocks = inlen / rate;
        for (size_t b = 0; b < blocks; b++) {
            for (int i = 0; i < rate; i++) sb[i] ^= in[b * rate + i];
            keccakf(st);
        }
        size_t rem = inlen - blocks * rate;
        for (size_t i = 0; i < rem; i++) sb[i] ^= in[blocks * rate + i];
        sb[rem] ^= 0x1F;
        sb[rate - 1] ^= 0x80;
        keccakf(st);
        squeezePos = 0;
    }
};

inline std::vector<uint8_t> cat(std::initializer_list<std::vector<uint8_t>> parts) {
    std::vector<uint8_t> out;
    for (auto& p : parts) out.insert(out.end(), p.begin(), p.end());
    return out;
}
inline std::vector<uint8_t> shake256Sum(int outLen, const std::vector<uint8_t>& in) {
    Shake s(136, in);
    std::vector<uint8_t> out(outLen);
    s.read(out.data(), outLen);
    return out;
}

// ---- parameters ------------------------------------------------------------

static const int32_t Q = 8380417;
static const int64_t Q64 = 8380417;
static const int NN = 256;
static const int DD = 13;
static const int64_t ROOT_ZETA = 1753;

inline int64_t modPow(int64_t base, int64_t exp, int64_t mod) {
    base %= mod;
    if (base < 0) base += mod;
    int64_t r = 1;
    while (exp > 0) {
        if (exp & 1) r = r * base % mod;
        base = base * base % mod;
        exp >>= 1;
    }
    return r;
}
inline int bitrev8(int x) {
    int r = 0;
    for (int i = 0; i < 8; i++) r = (r << 1) | ((x >> i) & 1);
    return r;
}
inline int bitlen(int x) {
    int n = 0;
    while (x > 0) { n++; x >>= 1; }
    return n;
}
inline const std::array<int32_t, 256>& zetas() {
    static const std::array<int32_t, 256> z = [] {
        std::array<int32_t, 256> a;
        for (int i = 0; i < NN; i++) a[i] = (int32_t)modPow(ROOT_ZETA, bitrev8(i), Q64);
        return a;
    }();
    return z;
}
inline int32_t ninv() {
    static const int32_t v = (int32_t)modPow(NN, Q64 - 2, Q64);
    return v;
}
inline int32_t modQ(int64_t a) {
    int64_t m = a % Q64;
    if (m < 0) m += Q64;
    return (int32_t)m;
}
inline int32_t centered(int32_t a) { return a > Q / 2 ? a - Q : a; }

struct Param {
    int k, l, eta, tau, omega, ctBytes;
    int32_t beta, gamma1, gamma2;
};
// level: 44, 65, or 87
inline Param paramFor(int level) {
    if (level == 44) return {4, 4, 2, 39, 80, 32, 39 * 2, 1 << 17, (Q - 1) / 88};
    if (level == 87) return {8, 7, 2, 60, 75, 64, 60 * 2, 1 << 19, (Q - 1) / 32};
    return {6, 5, 4, 49, 55, 48, 49 * 4, 1 << 19, (Q - 1) / 32}; // 65
}
inline int levelFromEnum(int levelEnum) { // 2->44, 3->65, 5->87
    if (levelEnum == 2) return 44;
    if (levelEnum == 5) return 87;
    return 65;
}

using Poly = std::array<int32_t, 256>;
using PolyVec = std::vector<Poly>;

// ---- NTT -------------------------------------------------------------------

inline void nttForward(Poly& a) {
    const auto& z = zetas();
    int kk = 0;
    for (int len = 128; len > 0; len >>= 1) {
        for (int start = 0; start < NN; start += 2 * len) {
            int64_t zeta = z[++kk];
            for (int j = start; j < start + len; j++) {
                int32_t t = (int32_t)(zeta * a[j + len] % Q64);
                a[j + len] = modQ((int64_t)a[j] - t);
                a[j] = modQ((int64_t)a[j] + t);
            }
        }
    }
}
inline void nttInverse(Poly& a) {
    const auto& z = zetas();
    int kk = NN;
    for (int len = 1; len < NN; len <<= 1) {
        for (int start = 0; start < NN; start += 2 * len) {
            int64_t zeta = Q - z[--kk];
            for (int j = start; j < start + len; j++) {
                int32_t t = a[j];
                a[j] = modQ((int64_t)t + a[j + len]);
                a[j + len] = modQ((int64_t)t - a[j + len]);
                a[j + len] = (int32_t)(zeta * a[j + len] % Q64);
            }
        }
    }
    int32_t ni = ninv();
    for (int j = 0; j < NN; j++) a[j] = (int32_t)((int64_t)ni * a[j] % Q64);
}
inline Poly nttCopy(const Poly& a) { Poly c = a; nttForward(c); return c; }
inline Poly pointwise(const Poly& a, const Poly& b) {
    Poly c;
    for (int i = 0; i < NN; i++) c[i] = (int32_t)((int64_t)a[i] * b[i] % Q64);
    return c;
}
inline void polyAddInto(Poly& dst, const Poly& src) {
    for (int i = 0; i < NN; i++) dst[i] = modQ((int64_t)dst[i] + src[i]);
}

// ---- rounding & hints ------------------------------------------------------

inline void power2Round(int32_t r, int32_t& r1, int32_t& r0) {
    r = modQ(r);
    int32_t a = r & ((1 << DD) - 1);
    if (a > (1 << (DD - 1))) a -= (1 << DD);
    r0 = a;
    r1 = (r - a) >> DD;
}
inline void decompose(int32_t r, int32_t gamma2, int32_t& r1, int32_t& r0) {
    r = modQ(r);
    int32_t alpha = 2 * gamma2;
    int32_t a = r % alpha;
    if (a > alpha / 2) a -= alpha;
    if (r - a == Q - 1) { r1 = 0; r0 = a - 1; }
    else { r1 = (r - a) / alpha; r0 = a; }
}
inline int32_t highBits(int32_t r, int32_t gamma2) { int32_t a, b; decompose(r, gamma2, a, b); return a; }
inline int32_t lowBits(int32_t r, int32_t gamma2) { int32_t a, b; decompose(r, gamma2, a, b); return b; }
inline int32_t useHint(int h, int32_t r, int32_t gamma2) {
    int32_t m = (Q - 1) / (2 * gamma2);
    int32_t r1, r0;
    decompose(r, gamma2, r1, r0);
    if (h == 0) return r1;
    if (r0 > 0) r1++; else r1--;
    r1 %= m;
    if (r1 < 0) r1 += m;
    return r1;
}
inline int32_t polyInfNorm(const Poly& p) {
    int32_t mx = 0;
    for (int i = 0; i < NN; i++) {
        int32_t a = centered(modQ(p[i]));
        if (a < 0) a = -a;
        if (a > mx) mx = a;
    }
    return mx;
}
inline int32_t polyVecInfNorm(const PolyVec& v) {
    int32_t mx = 0;
    for (auto& p : v) { int32_t n = polyInfNorm(p); if (n > mx) mx = n; }
    return mx;
}

// ---- sampling --------------------------------------------------------------

inline Poly rejNTTPoly(const std::vector<uint8_t>& rho, uint8_t s, uint8_t r) {
    Shake st(168, cat({rho, {s, r}}));
    Poly p{};
    int i = 0;
    uint8_t b[3];
    while (i < NN) {
        st.read(b, 3);
        int d = (int)b[0] | ((int)b[1] << 8) | (((int)b[2] & 0x7F) << 16);
        if (d < Q) p[i++] = d;
    }
    return p;
}
inline std::vector<Poly> expandA(const std::vector<uint8_t>& rho, int k, int l) {
    std::vector<Poly> all(k * l);
    for (int r = 0; r < k; r++)
        for (int s = 0; s < l; s++) all[r * l + s] = rejNTTPoly(rho, (uint8_t)s, (uint8_t)r);
    return all;
}
inline bool coeffFromHalfByte(int z, int eta, int32_t& out) {
    if (eta == 2) {
        if (z < 15) { out = 2 - (z % 5); return true; }
        return false;
    }
    if (z < 9) { out = 4 - z; return true; }
    return false;
}
inline Poly rejBoundedPoly(const std::vector<uint8_t>& rhoP, uint16_t nonce, int eta) {
    Shake st(136, cat({rhoP, {(uint8_t)(nonce & 0xFF), (uint8_t)(nonce >> 8)}}));
    Poly p{};
    int i = 0;
    while (i < NN) {
        uint8_t b = st.readByte();
        int32_t v;
        if (coeffFromHalfByte(b & 0x0F, eta, v)) { p[i++] = v; if (i >= NN) break; }
        if (coeffFromHalfByte(b >> 4, eta, v)) { p[i++] = v; }
    }
    return p;
}

struct BitReader {
    const uint8_t* in; size_t len; size_t pos = 0; uint64_t cur = 0; int nbits = 0;
    uint32_t read(int bits) {
        while (nbits < bits) {
            uint8_t b = pos < len ? in[pos++] : 0;
            cur |= (uint64_t)b << nbits;
            nbits += 8;
        }
        uint32_t v = (uint32_t)(cur & (((uint64_t)1 << bits) - 1));
        cur >>= bits;
        nbits -= bits;
        return v;
    }
};
struct BitWriter {
    std::vector<uint8_t> out; uint64_t cur = 0; int nbits = 0;
    void write(uint32_t val, int bits) {
        cur |= (uint64_t)val << nbits;
        nbits += bits;
        while (nbits >= 8) { out.push_back((uint8_t)cur); cur >>= 8; nbits -= 8; }
    }
};

inline Poly bitUnpack(const uint8_t* buf, size_t len, int32_t b, int bits) {
    BitReader r{buf, len};
    Poly p;
    for (int i = 0; i < NN; i++) p[i] = b - (int32_t)r.read(bits);
    return p;
}
inline std::vector<uint8_t> bitPack(const Poly& p, int32_t b, int bits) {
    BitWriter w;
    for (int i = 0; i < NN; i++) w.write((uint32_t)(b - p[i]), bits);
    return w.out;
}
inline Poly simpleBitUnpack(const uint8_t* buf, size_t len, int bits) {
    BitReader r{buf, len};
    Poly p;
    for (int i = 0; i < NN; i++) p[i] = (int32_t)r.read(bits);
    return p;
}
inline std::vector<uint8_t> simpleBitPack(const Poly& p, int bits) {
    BitWriter w;
    for (int i = 0; i < NN; i++) w.write((uint32_t)p[i], bits);
    return w.out;
}

inline PolyVec expandMask(const std::vector<uint8_t>& rhoPP, int kappa, int l, int32_t gamma1) {
    int width = bitlen((int)(2 * gamma1 - 1));
    int bytesPerPoly = NN * width / 8;
    PolyVec y(l);
    std::vector<uint8_t> buf(bytesPerPoly);
    for (int i = 0; i < l; i++) {
        uint16_t nonce = (uint16_t)(kappa + i);
        Shake st(136, cat({rhoPP, {(uint8_t)(nonce & 0xFF), (uint8_t)(nonce >> 8)}}));
        st.read(buf.data(), bytesPerPoly);
        y[i] = bitUnpack(buf.data(), buf.size(), gamma1, width);
    }
    return y;
}
inline Poly sampleInBall(const std::vector<uint8_t>& ctilde, int tau) {
    Shake st(136, ctilde);
    uint8_t sb[8];
    st.read(sb, 8);
    uint64_t signs = 0;
    for (int i = 0; i < 8; i++) signs |= (uint64_t)sb[i] << (8 * i);
    Poly c{};
    for (int i = NN - tau; i < NN; i++) {
        uint8_t j;
        do { j = st.readByte(); } while ((int)j > i);
        c[i] = c[j];
        c[j] = (signs & 1) ? -1 : 1;
        signs >>= 1;
    }
    return c;
}

inline std::vector<uint8_t> hintPack(const PolyVec& h, int omega, int k) {
    std::vector<uint8_t> out(omega + k, 0);
    int idx = 0;
    for (int i = 0; i < k; i++) {
        for (int j = 0; j < NN; j++)
            if (h[i][j] != 0) { if (idx >= omega) return {}; out[idx++] = (uint8_t)j; }
        out[omega + i] = (uint8_t)idx;
    }
    return out;
}
inline bool hintUnpack(const uint8_t* buf, int omega, int k, PolyVec& h) {
    h.assign(k, Poly{});
    int idx = 0;
    for (int i = 0; i < k; i++) {
        int end = buf[omega + i];
        if (end < idx || end > omega) return false;
        int first = idx;
        for (; idx < end; idx++) {
            if (idx > first && buf[idx] <= buf[idx - 1]) return false;
            h[i][buf[idx]] = 1;
        }
    }
    for (; idx < omega; idx++)
        if (buf[idx] != 0) return false;
    return true;
}

// ---- key structures & serialization ---------------------------------------

struct PublicKey { std::array<uint8_t, 32> rho; PolyVec t1; };
struct SecretKey {
    std::array<uint8_t, 32> rho, key;
    std::array<uint8_t, 64> tr;
    PolyVec s1, s2, t0;
};

inline std::vector<uint8_t> encodePK(const Param& p, const PublicKey& pk) {
    std::vector<uint8_t> out(pk.rho.begin(), pk.rho.end());
    for (int i = 0; i < p.k; i++) {
        auto b = simpleBitPack(pk.t1[i], 10);
        out.insert(out.end(), b.begin(), b.end());
    }
    return out;
}
inline bool decodePK(const Param& p, const uint8_t* data, size_t len, PublicKey& pk) {
    size_t expected = 32 + (size_t)p.k * (NN * 10 / 8);
    if (len != expected) return false;
    std::copy(data, data + 32, pk.rho.begin());
    pk.t1.assign(p.k, Poly{});
    size_t off = 32;
    int bytesPerPoly = NN * 10 / 8;
    for (int i = 0; i < p.k; i++) {
        pk.t1[i] = simpleBitUnpack(data + off, bytesPerPoly, 10);
        off += bytesPerPoly;
    }
    return true;
}
inline std::vector<uint8_t> encodeSK(const Param& p, const SecretKey& sk) {
    int etabits = bitlen(2 * p.eta);
    std::vector<uint8_t> out;
    out.insert(out.end(), sk.rho.begin(), sk.rho.end());
    out.insert(out.end(), sk.key.begin(), sk.key.end());
    out.insert(out.end(), sk.tr.begin(), sk.tr.end());
    for (int i = 0; i < p.l; i++) { auto b = bitPack(sk.s1[i], p.eta, etabits); out.insert(out.end(), b.begin(), b.end()); }
    for (int i = 0; i < p.k; i++) { auto b = bitPack(sk.s2[i], p.eta, etabits); out.insert(out.end(), b.begin(), b.end()); }
    for (int i = 0; i < p.k; i++) { auto b = bitPack(sk.t0[i], 1 << (DD - 1), DD); out.insert(out.end(), b.begin(), b.end()); }
    return out;
}
inline bool decodeSK(const Param& p, const uint8_t* data, size_t len, SecretKey& sk) {
    int etabits = bitlen(2 * p.eta);
    int etaBytes = NN * etabits / 8;
    int t0Bytes = NN * DD / 8;
    size_t expected = 128 + (size_t)(p.l + p.k) * etaBytes + (size_t)p.k * t0Bytes;
    if (len != expected) return false;
    std::copy(data, data + 32, sk.rho.begin());
    std::copy(data + 32, data + 64, sk.key.begin());
    std::copy(data + 64, data + 128, sk.tr.begin());
    size_t off = 128;
    sk.s1.assign(p.l, Poly{});
    for (int i = 0; i < p.l; i++) { sk.s1[i] = bitUnpack(data + off, etaBytes, p.eta, etabits); off += etaBytes; }
    sk.s2.assign(p.k, Poly{});
    for (int i = 0; i < p.k; i++) { sk.s2[i] = bitUnpack(data + off, etaBytes, p.eta, etabits); off += etaBytes; }
    sk.t0.assign(p.k, Poly{});
    for (int i = 0; i < p.k; i++) { sk.t0[i] = bitUnpack(data + off, t0Bytes, 1 << (DD - 1), DD); off += t0Bytes; }
    return true;
}

// ---- keygen / sign / verify (serialized) -----------------------------------

inline void keyGenInternal(const Param& p, const uint8_t xi[32], PublicKey& pk, SecretKey& sk) {
    std::vector<uint8_t> seedIn(xi, xi + 32);
    seedIn.push_back((uint8_t)p.k);
    seedIn.push_back((uint8_t)p.l);
    auto expanded = shake256Sum(128, seedIn);
    std::vector<uint8_t> rho(expanded.begin(), expanded.begin() + 32);
    std::vector<uint8_t> rhoP(expanded.begin() + 32, expanded.begin() + 96);
    std::vector<uint8_t> K(expanded.begin() + 96, expanded.begin() + 128);

    auto A = expandA(rho, p.k, p.l);
    PolyVec s1(p.l), s2(p.k);
    for (int i = 0; i < p.l; i++) s1[i] = rejBoundedPoly(rhoP, (uint16_t)i, p.eta);
    for (int i = 0; i < p.k; i++) s2[i] = rejBoundedPoly(rhoP, (uint16_t)(p.l + i), p.eta);

    std::vector<Poly> s1hat(p.l);
    for (int j = 0; j < p.l; j++) s1hat[j] = nttCopy(s1[j]);

    PolyVec t1(p.k), t0(p.k);
    for (int i = 0; i < p.k; i++) {
        Poly acc{};
        for (int j = 0; j < p.l; j++) polyAddInto(acc, pointwise(A[i * p.l + j], s1hat[j]));
        nttInverse(acc);
        polyAddInto(acc, s2[i]);
        for (int c = 0; c < NN; c++) power2Round(acc[c], t1[i][c], t0[i][c]);
    }

    std::copy(rho.begin(), rho.end(), pk.rho.begin());
    pk.t1 = t1;
    std::copy(rho.begin(), rho.end(), sk.rho.begin());
    std::copy(K.begin(), K.end(), sk.key.begin());
    sk.s1 = s1; sk.s2 = s2; sk.t0 = t0;
    auto tr = shake256Sum(64, encodePK(p, pk));
    std::copy(tr.begin(), tr.end(), sk.tr.begin());
}

inline std::vector<uint8_t> framed(const uint8_t* ctx, size_t ctxlen, const uint8_t* msg, size_t msglen) {
    std::vector<uint8_t> out;
    out.push_back(0x00);
    out.push_back((uint8_t)ctxlen);
    out.insert(out.end(), ctx, ctx + ctxlen);
    out.insert(out.end(), msg, msg + msglen);
    return out;
}

// signCore produces a signature; rnd is 32 bytes (use zeros for deterministic).
inline std::vector<uint8_t> signCore(const Param& p, const SecretKey& sk,
                                     const uint8_t* msg, size_t msglen,
                                     const uint8_t* ctx, size_t ctxlen,
                                     const uint8_t rnd[32]) {
    auto A = expandA(std::vector<uint8_t>(sk.rho.begin(), sk.rho.end()), p.k, p.l);
    auto mu = shake256Sum(64, cat({std::vector<uint8_t>(sk.tr.begin(), sk.tr.end()), framed(ctx, ctxlen, msg, msglen)}));
    auto rhoPP = shake256Sum(64, cat({std::vector<uint8_t>(sk.key.begin(), sk.key.end()),
                                      std::vector<uint8_t>(rnd, rnd + 32), mu}));

    std::vector<Poly> s1hat(p.l), s2hat(p.k), t0hat(p.k);
    for (int j = 0; j < p.l; j++) s1hat[j] = nttCopy(sk.s1[j]);
    for (int i = 0; i < p.k; i++) { s2hat[i] = nttCopy(sk.s2[i]); t0hat[i] = nttCopy(sk.t0[i]); }

    int zbits = bitlen((int)(2 * p.gamma1 - 1));
    int w1bits = bitlen((int)((Q - 1) / (2 * p.gamma2)) - 1);
    int sigSize = p.ctBytes + p.l * (NN * zbits / 8) + p.omega + p.k;

    for (int kappa = 0; kappa < 1000 * p.l; kappa += p.l) {
        auto y = expandMask(rhoPP, kappa, p.l, p.gamma1);
        std::vector<Poly> yhat(p.l);
        for (int j = 0; j < p.l; j++) yhat[j] = nttCopy(y[j]);

        PolyVec w(p.k), w1(p.k);
        for (int i = 0; i < p.k; i++) {
            Poly acc{};
            for (int j = 0; j < p.l; j++) polyAddInto(acc, pointwise(A[i * p.l + j], yhat[j]));
            nttInverse(acc);
            w[i] = acc;
            for (int c = 0; c < NN; c++) w1[i][c] = highBits(acc[c], p.gamma2);
        }

        std::vector<uint8_t> w1packed;
        for (int i = 0; i < p.k; i++) { auto b = simpleBitPack(w1[i], w1bits); w1packed.insert(w1packed.end(), b.begin(), b.end()); }
        auto ctilde = shake256Sum(p.ctBytes, cat({mu, w1packed}));
        auto c = sampleInBall(ctilde, p.tau);
        auto chat = nttCopy(c);

        PolyVec z(p.l);
        for (int j = 0; j < p.l; j++) {
            Poly cs1 = pointwise(chat, s1hat[j]);
            nttInverse(cs1);
            for (int cc = 0; cc < NN; cc++) z[j][cc] = centered(modQ((int64_t)y[j][cc] + cs1[cc]));
        }
        if (polyVecInfNorm(z) >= p.gamma1 - p.beta) continue;

        PolyVec wcs2(p.k);
        int32_t r0norm = 0;
        for (int i = 0; i < p.k; i++) {
            Poly cs2 = pointwise(chat, s2hat[i]);
            nttInverse(cs2);
            for (int cc = 0; cc < NN; cc++) {
                wcs2[i][cc] = modQ((int64_t)w[i][cc] - cs2[cc]);
                int32_t lb = lowBits(wcs2[i][cc], p.gamma2);
                if (lb < 0) lb = -lb;
                if (lb > r0norm) r0norm = lb;
            }
        }
        if (r0norm >= p.gamma2 - p.beta) continue;

        PolyVec h(p.k);
        int32_t ct0norm = 0;
        int hintCount = 0;
        for (int i = 0; i < p.k; i++) {
            Poly ct0 = pointwise(chat, t0hat[i]);
            nttInverse(ct0);
            for (int cc = 0; cc < NN; cc++) {
                int32_t ct0c = centered(modQ(ct0[cc]));
                int32_t a = ct0c < 0 ? -ct0c : ct0c;
                if (a > ct0norm) ct0norm = a;
                int32_t hi1 = highBits(modQ((int64_t)wcs2[i][cc] + ct0[cc]), p.gamma2);
                int32_t hi2 = highBits(wcs2[i][cc], p.gamma2);
                if (hi1 != hi2) { h[i][cc] = 1; hintCount++; }
            }
        }
        if (ct0norm >= p.gamma2) continue;
        if (hintCount > p.omega) continue;

        std::vector<uint8_t> sig;
        sig.reserve(sigSize);
        sig.insert(sig.end(), ctilde.begin(), ctilde.end());
        for (int j = 0; j < p.l; j++) { auto b = bitPack(z[j], p.gamma1, zbits); sig.insert(sig.end(), b.begin(), b.end()); }
        auto hb = hintPack(h, p.omega, p.k);
        if (hb.empty()) continue;
        sig.insert(sig.end(), hb.begin(), hb.end());
        return sig;
    }
    return {};
}

inline bool verifyCore(const Param& p, const PublicKey& pk,
                       const uint8_t* msg, size_t msglen,
                       const uint8_t* ctx, size_t ctxlen,
                       const uint8_t* sig, size_t siglen) {
    int zbits = bitlen((int)(2 * p.gamma1 - 1));
    int w1bits = bitlen((int)((Q - 1) / (2 * p.gamma2)) - 1);
    int zBytesPerPoly = NN * zbits / 8;
    int sigSize = p.ctBytes + p.l * zBytesPerPoly + p.omega + p.k;
    if ((int)siglen != sigSize) return false;

    int off = 0;
    std::vector<uint8_t> ctilde(sig, sig + p.ctBytes);
    off += p.ctBytes;
    PolyVec z(p.l);
    for (int j = 0; j < p.l; j++) {
        z[j] = bitUnpack(sig + off, zBytesPerPoly, p.gamma1, zbits);
        off += zBytesPerPoly;
    }
    PolyVec h;
    if (!hintUnpack(sig + off, p.omega, p.k, h)) return false;
    if (polyVecInfNorm(z) >= p.gamma1 - p.beta) return false;

    auto A = expandA(std::vector<uint8_t>(pk.rho.begin(), pk.rho.end()), p.k, p.l);
    auto tr = shake256Sum(64, encodePK(p, pk));
    auto mu = shake256Sum(64, cat({tr, framed(ctx, ctxlen, msg, msglen)}));
    auto c = sampleInBall(ctilde, p.tau);
    auto chat = nttCopy(c);

    std::vector<Poly> zhat(p.l);
    for (int j = 0; j < p.l; j++) zhat[j] = nttCopy(z[j]);
    std::vector<Poly> t1hat(p.k);
    for (int i = 0; i < p.k; i++) {
        Poly scaled;
        for (int cc = 0; cc < NN; cc++) scaled[cc] = modQ((int64_t)pk.t1[i][cc] << DD);
        t1hat[i] = nttCopy(scaled);
    }

    std::vector<uint8_t> w1packed;
    for (int i = 0; i < p.k; i++) {
        Poly acc{};
        for (int j = 0; j < p.l; j++) polyAddInto(acc, pointwise(A[i * p.l + j], zhat[j]));
        Poly ct1 = pointwise(chat, t1hat[i]);
        for (int cc = 0; cc < NN; cc++) acc[cc] = modQ((int64_t)acc[cc] - ct1[cc]);
        nttInverse(acc);
        Poly w1;
        for (int cc = 0; cc < NN; cc++) w1[cc] = useHint(h[i][cc], acc[cc], p.gamma2);
        auto b = simpleBitPack(w1, w1bits);
        w1packed.insert(w1packed.end(), b.begin(), b.end());
    }
    auto ctildePrime = shake256Sum(p.ctBytes, cat({mu, w1packed}));
    if (ctildePrime.size() != ctilde.size()) return false;
    uint8_t diff = 0;
    for (size_t i = 0; i < ctilde.size(); i++) diff |= ctilde[i] ^ ctildePrime[i];
    return diff == 0;
}

// ---- public serialized API (used by the CMLDSA fallback) -------------------

inline void randomBytes(uint8_t* out, size_t len) {
    std::random_device rd;
    size_t i = 0;
    while (i < len) {
        uint32_t v = rd();
        for (int b = 0; b < 4 && i < len; b++) out[i++] = (uint8_t)(v >> (8 * b));
    }
}

// GenerateKeyPair fills pkOut/skOut with serialized keys for the given level
// enum (2/3/5). Returns true on success.
inline bool GenerateKeyPair(int levelEnum, std::vector<uint8_t>& pkOut, std::vector<uint8_t>& skOut) {
    Param p = paramFor(levelFromEnum(levelEnum));
    uint8_t xi[32];
    randomBytes(xi, 32);
    PublicKey pk; SecretKey sk;
    keyGenInternal(p, xi, pk, sk);
    pkOut = encodePK(p, pk);
    skOut = encodeSK(p, sk);
    return true;
}

// Sign writes a signature over (ctx, msg) using a serialized secret key.
inline bool Sign(int levelEnum, const uint8_t* sk, size_t sklen,
                 const uint8_t* msg, size_t msglen,
                 const uint8_t* ctx, size_t ctxlen,
                 uint8_t* sigOut) {
    Param p = paramFor(levelFromEnum(levelEnum));
    SecretKey s;
    if (!decodeSK(p, sk, sklen, s)) return false;
    uint8_t rnd[32];
    randomBytes(rnd, 32);
    auto sig = signCore(p, s, msg, msglen, ctx, ctxlen, rnd);
    if (sig.empty()) return false;
    std::memcpy(sigOut, sig.data(), sig.size());
    return true;
}

// Verify checks a signature using a serialized public key.
inline bool Verify(int levelEnum, const uint8_t* pk, size_t pklen,
                   const uint8_t* msg, size_t msglen,
                   const uint8_t* ctx, size_t ctxlen,
                   const uint8_t* sig, size_t siglen) {
    Param p = paramFor(levelFromEnum(levelEnum));
    PublicKey k;
    if (!decodePK(p, pk, pklen, k)) return false;
    return verifyCore(p, k, msg, msglen, ctx, ctxlen, sig, siglen);
}

} // namespace fips204
} // namespace crypto
} // namespace bhc

#endif // BHC_CRYPTO_MLDSA_FIPS204_H
