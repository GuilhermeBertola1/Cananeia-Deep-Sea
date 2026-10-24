#pragma once
#include <stdint.h>
#include <stddef.h>

#define PASSWORD_SALT_LEN 16
#define PASSWORD_HASH_LEN 32
#define PASSWORD_ITERATIONS 4096

int pbkdf2_sha256(const uint8_t *password, size_t password_len, const uint8_t *salt, size_t salt_len,
                  uint32_t iterations, uint8_t *out, size_t out_len);

void password_random(uint8_t *out, size_t len);

static inline int password_hash(const uint8_t *password, size_t len, const uint8_t salt[PASSWORD_SALT_LEN],
                                uint8_t out[PASSWORD_HASH_LEN]) {
    return pbkdf2_sha256(password, len, salt, PASSWORD_SALT_LEN, PASSWORD_ITERATIONS, out, PASSWORD_HASH_LEN);
}

static inline int password_equal(const uint8_t a[PASSWORD_HASH_LEN], const uint8_t b[PASSWORD_HASH_LEN]) {
    uint8_t diff = 0;
    for (int i = 0; i < PASSWORD_HASH_LEN; i++) diff |= a[i] ^ b[i];
    return diff == 0;
}