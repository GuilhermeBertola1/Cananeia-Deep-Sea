// Confere o PBKDF2 do PC com os vetores de teste do RFC 7914 (seção 11).
// Linux: cc -I../components/game/include test_password.c password_host.c -o test_password
// Windows: cl /nologo /utf-8 /I..\components\game\include test_password.c password_host.c
#include <stdio.h>
#include <string.h>
#include "password.h"

static int check(const char *pw, const char *salt, uint32_t iterations, const char *expected_hex) {
    uint8_t out[64];
    char hex[129];
    pbkdf2_sha256((const uint8_t *)pw, strlen(pw), (const uint8_t *)salt, strlen(salt), iterations, out, 64);
    for (int i = 0; i < 64; i++) sprintf(hex + i * 2, "%02x", out[i]);
    int ok = strcmp(hex, expected_hex) == 0;
    printf("%s  senha=\"%s\" sal=\"%s\" c=%u\n", ok ? "OK  " : "ERRO", pw, salt, iterations);
    return ok;
}

int main(void) {
    int ok = check("passwd", "salt", 1,
                   "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc"
                   "49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783");
    ok &= check("Password", "NaCl", 80000,
                "4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56"
                "a1d425a1225833549adb841b51c9b3176a272bdebba1d078478f62b397f33c8d");
    puts(ok ? "PBKDF2 certo." : "PBKDF2 ERRADO!");
    return ok ? 0 : 1;
}