// Senhas no ESP32: PBKDF2 do mbedTLS pela API PSA Crypto (o ESP-IDF 6
// usa o mbedTLS 4, onde a API PSA substituiu as funções antigas).
// Só entra na build do ESP-IDF; o PC usa server/host/password_host.c.
#include "esp_random.h"
#include "psa/crypto.h"
#include "password.h"

int pbkdf2_sha256(const uint8_t *password, size_t password_len, const uint8_t *salt, size_t salt_len,
                  uint32_t iterations, uint8_t *out, size_t out_len) {
    if (psa_crypto_init() != PSA_SUCCESS) return 0; // o ESP-IDF já inicia; chamar de novo não faz mal
    psa_key_derivation_operation_t op = PSA_KEY_DERIVATION_OPERATION_INIT;
    // A ordem das entradas é obrigatória no PBKDF2: custo, sal, senha.
    psa_status_t st = psa_key_derivation_setup(&op, PSA_ALG_PBKDF2_HMAC(PSA_ALG_SHA_256));
    if (st == PSA_SUCCESS) st = psa_key_derivation_input_integer(&op, PSA_KEY_DERIVATION_INPUT_COST, iterations);
    if (st == PSA_SUCCESS) st = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_SALT, salt, salt_len);
    if (st == PSA_SUCCESS)
        st = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_PASSWORD, password, password_len);
    if (st == PSA_SUCCESS) st = psa_key_derivation_output_bytes(&op, out, out_len);
    psa_key_derivation_abort(&op);
    return st == PSA_SUCCESS;
}

void password_random(uint8_t *out, size_t len) {
    esp_fill_random(out, len); // gerador de hardware do ESP32
}