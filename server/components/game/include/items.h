// Itens procedurais. Um item salvo é só {semente, tipo}: 8 bytes. Tudo o
// mais (raridade, afixos, números) sai da semente pela mesma conta, então
// o mesmo item é sempre igual, sem precisar guardar os detalhes.
//
// A mesma tabela de nomes existe em client/src/items.odin; se mudar a ordem
// aqui, mude lá também (a ordem é o id enviado na rede).
#pragma once
#include <stdint.h>

typedef enum {
    STAT_SPEED = 0,  // células por segundo
    STAT_HP,         // vida do barco (usada a partir da v0.5)
    STAT_DAMAGE,     // dano da arma (usado a partir da v0.5)
    STAT_PRODUCTION, // % a mais de chance de pescar
    STAT_LUCK,       // chance de pescar baú com item
    STAT_COUNT,
} stat_t;

typedef enum { RARITY_COMMON = 0, RARITY_RARE, RARITY_EPIC, RARITY_LEGENDARY, RARITY_COUNT } rarity_t;

// Em que slot do barco o item entra.
typedef enum { KIND_WEAPON = 0, KIND_TOOL, KIND_BOOST } item_kind_t;

typedef enum {
    ITEM_NONE = 0, // slot vazio: perfil zerado já começa com a bolsa vazia
    ITEM_ARPAO,
    ITEM_MOSQUETE,
    ITEM_CANHAO,
    ITEM_VARA,
    ITEM_REDE,
    ITEM_ARRASTO,
    ITEM_MOTOR,
    ITEM_CASCO,
    ITEM_AMULETO,
    ITEM_ISCA,
    ITEM_BASE_COUNT,
} item_base_id_t;

typedef enum {
    AFFIX_VELOZ = 0,
    AFFIX_ROBUSTO,
    AFFIX_AFIADO,
    AFFIX_PESCADOR,
    AFFIX_SORTE,
    AFFIX_COUNT,
} affix_id_t;

#define AFFIX_NONE 0xFF
#define MAX_AFFIXES 3
// Encanto da sereia: cada nível soma 25% (no mínimo +1) em cada stat do item.
#define ENCHANT_MAX 3
#define ENCHANT_PCT 25

typedef struct {
    uint32_t seed;
    uint8_t base; // item_base_id_t (ITEM_NONE = vazio)
} item_t;

typedef struct {
    const char *name;
    uint8_t kind;      // item_kind_t
    uint8_t stat;      // stat_t principal
    int16_t min, max;  // faixa do stat principal num item comum
} item_base_t;

typedef struct {
    const char *name;
    uint8_t stat;
    int16_t min, max;
} affix_t;

// O item "aberto": o que a semente gera.
typedef struct {
    uint8_t base, rarity;
    uint8_t affix[MAX_AFFIXES]; // AFFIX_NONE nos que sobram
    int16_t stats[STAT_COUNT];  // stat principal + afixos, já somados
} item_info_t;

extern const item_base_t ITEM_BASES[ITEM_BASE_COUNT];
extern const affix_t AFFIXES[AFFIX_COUNT];

item_t item_new(uint8_t base, uint32_t seed);
item_info_t item_info(item_t item);
// O mesmo item_info, com o encanto por cima. O encanto não está na semente:
// fica guardado no perfil, ao lado do item (profile_t.enchant).
item_info_t item_info_enchanted(item_t item, uint8_t level);
