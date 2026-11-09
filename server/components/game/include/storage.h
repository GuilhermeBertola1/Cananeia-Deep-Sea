// Onde os perfis ficam salvos. Cada plataforma implementa:
// ESP32 -> NVS (server/main/storage_nvs.c), PC -> arquivo (server/host/storage_file.c).
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "fish.h"
#include "password.h"
#include "items.h"
#include "protocol.h"

#define MAX_PROFILES 256
#define BAG_SIZE 12
// Slots do barco: 0 arma, 1 ferramenta de pesca, 2 a 4 boosts.
#define EQUIP_SLOTS 5
#define EQUIP_WEAPON 0
#define EQUIP_TOOL 1
#define EQUIP_BOOST 2

typedef struct {
    char name[NAME_MAX_LEN + 1];
    uint8_t used;
    uint32_t money; // em centavos
    uint16_t hold[FISH_COUNT];
    uint16_t x, y; // onde parou (0, 0 = nascer no porto)
    uint8_t salt[PASSWORD_SALT_LEN];      // aleatório, um por perfil
    uint8_t pass_hash[PASSWORD_HASH_LEN]; // PBKDF2(senha, sal): a senha mesmo nunca é guardada
    item_t bag[BAG_SIZE];
    uint8_t equipped[EQUIP_SLOTS];
    uint8_t boat[3];
    uint16_t damage;
} profile_t;

#define PROFILE_V03_SIZE 200

static inline void profile_upgrade(profile_t *p, size_t old_size) {
    size_t keep = old_size == PROFILE_V03_SIZE ? offsetof(profile_t, boat) : old_size;
    memset((uint8_t *)p + keep, 0, sizeof(profile_t) - keep);
}

// Carrega todos os perfis salvos em `out` (zerado antes). Devolve quantos achou.
int storage_load_all(profile_t *out, int max);
void storage_save(int index, const profile_t *p);
