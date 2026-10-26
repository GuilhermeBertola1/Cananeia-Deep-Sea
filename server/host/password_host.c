// Senhas no servidor de teste: SHA-256, HMAC e PBKDF2 em C puro, para não
// depender de biblioteca no PC. No ESP32 quem faz isso é o mbedTLS
// (components/game/password_psa.c); os dois dão o mesmo resultado.
#ifdef _WIN32
#define _CRT_RAND_S // libera o rand_s(), aleatório do sistema
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "password.h"

// --- SHA-256 (FIPS 180-4) ----------------------------------------------------

typedef struct {
    uint32_t h[8];
    uint8_t block[64];
    size_t used;    // bytes no bloco atual
    uint64_t total; // bytes vistos
} sha256_t;

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static uint32_t ror(uint32_t v, int n) { return (v >> n) | (v << (32 - n)); }

static void sha256_block(sha256_t *s, const uint8_t *b) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t)b[i * 4] << 24 | (uint32_t)b[i * 4 + 1] << 16 | (uint32_t)b[i * 4 + 2] << 8 | b[i * 4 + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = s->h[0], bb = s->h[1], c = s->h[2], d = s->h[3];
    uint32_t e = s->h[4], f = s->h[5], g = s->h[6], h = s->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + (ror(e, 6) ^ ror(e, 11) ^ ror(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
        uint32_t t2 = (ror(a, 2) ^ ror(a, 13) ^ ror(a, 22)) + ((a & bb) ^ (a & c) ^ (bb & c));
        h = g, g = f, f = e, e = d + t1;
        d = c, c = bb, bb = a, a = t1 + t2;
    }
    s->h[0] += a, s->h[1] += bb, s->h[2] += c, s->h[3] += d;
    s->h[4] += e, s->h[5] += f, s->h[6] += g, s->h[7] += h;
}

static void sha256_init(sha256_t *s) {
    static const uint32_t H0[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(s->h, H0, sizeof H0);
    s->used = 0;
    s->total = 0;
}

static void sha256_update(sha256_t *s, const uint8_t *data, size_t len) {
    s->total += len;
    while (len--) {
        s->block[s->used++] = *data++;
        if (s->used == 64) {
            sha256_block(s, s->block);
            s->used = 0;
        }
    }
}

static void sha256_final(sha256_t *s, uint8_t out[32]) {
    uint64_t bits = s->total * 8;
    uint8_t pad = 0x80;
    sha256_update(s, &pad, 1);
    pad = 0;
    while (s->used != 56) sha256_update(s, &pad, 1);
    uint8_t len_be[8];
    for (int i = 0; i < 8; i++) len_be[i] = (uint8_t)(bits >> (56 - 8 * i));
    sha256_update(s, len_be, 8);
    for (int i = 0; i < 8; i++) {
        out[i * 4] = (uint8_t)(s->h[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(s->h[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(s->h[i] >> 8);
        out[i * 4 + 3] = (uint8_t)s->h[i];
    }
}

// --- HMAC-SHA256 (RFC 2104) --------------------------------------------------

typedef struct {
    sha256_t inner, outer; // já com a chave misturada, para reaproveitar
} hmac_t;

static void hmac_init(hmac_t *m, const uint8_t *key, size_t key_len) {
    uint8_t k[64] = {0}, pad[64];
    if (key_len > 64) {
        sha256_init(&m->inner);
        sha256_update(&m->inner, key, key_len);
        sha256_final(&m->inner, k);
    } else {
        memcpy(k, key, key_len);
    }
    for (int i = 0; i < 64; i++) pad[i] = k[i] ^ 0x36;
    sha256_init(&m->inner);
    sha256_update(&m->inner, pad, 64);
    for (int i = 0; i < 64; i++) pad[i] = k[i] ^ 0x5c;
    sha256_init(&m->outer);
    sha256_update(&m->outer, pad, 64);
}

// HMAC(chave, a || b). `m` não muda, então serve para várias contas.
static void hmac(const hmac_t *m, const uint8_t *a, size_t a_len, const uint8_t *b, size_t b_len, uint8_t out[32]) {
    sha256_t s = m->inner;
    sha256_update(&s, a, a_len);
    sha256_update(&s, b, b_len);
    sha256_final(&s, out);
    s = m->outer;
    sha256_update(&s, out, 32);
    sha256_final(&s, out);
}

// --- PBKDF2 (RFC 8018) -------------------------------------------------------

int pbkdf2_sha256(const uint8_t *password, size_t password_len, const uint8_t *salt, size_t salt_len,
                  uint32_t iterations, uint8_t *out, size_t out_len) {
    if (iterations < 1) return 0;
    hmac_t m;
    hmac_init(&m, password, password_len);
    // A saída vem em blocos de 32 bytes: T_i = U_1 ^ U_2 ^ ... ^ U_c
    for (uint32_t block = 1; out_len > 0; block++) {
        uint8_t index[4] = {block >> 24, block >> 16, block >> 8, block};
        uint8_t u[32], t[32];
        hmac(&m, salt, salt_len, index, 4, u); // U_1 = HMAC(senha, sal || i)
        memcpy(t, u, 32);
        for (uint32_t i = 1; i < iterations; i++) {
            hmac(&m, u, 32, NULL, 0, u); // U_n = HMAC(senha, U_n-1)
            for (int j = 0; j < 32; j++) t[j] ^= u[j];
        }
        size_t n = out_len < 32 ? out_len : 32;
        memcpy(out, t, n);
        out += n;
        out_len -= n;
    }
    return 1;
}

void password_random(uint8_t *out, size_t len) {
#ifdef _WIN32
    for (size_t i = 0; i < len; i++) {
        unsigned int r;
        rand_s(&r);
        out[i] = (uint8_t)r;
    }
#else
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f || fread(out, 1, len, f) != len) {
        fprintf(stderr, "sem /dev/urandom\n");
        exit(1);
    }
    fclose(f);
#endif
}