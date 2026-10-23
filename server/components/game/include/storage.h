// Onde os perfis ficam salvos. Cada plataforma implementa:
// ESP32 -> NVS (server/main/storage_nvs.c), PC -> arquivo (server/host/storage_file.c).
#pragma once
#include <stdint.h>
#include "fish.h"
#include "protocol.h"

#define MAX_PROFILES 256

typedef struct {
    char name[NAME_MAX_LEN + 1];
    uint8_t used;
    uint32_t money; // em centavos
    uint16_t hold[FISH_COUNT];
    uint16_t x, y; // onde parou (0, 0 = nascer no porto)
} profile_t;

// Carrega todos os perfis salvos em `out` (zerado antes). Devolve quantos achou.
int storage_load_all(profile_t *out, int max);
void storage_save(int index, const profile_t *p);
