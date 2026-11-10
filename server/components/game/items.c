#include "items.h"

// Faixa de cada stat num item comum. A raridade multiplica depois.
// A posição 0 (ITEM_NONE) fica vazia.
const item_base_t ITEM_BASES[ITEM_BASE_COUNT] = {
    [ITEM_ARPAO] = {"Arpão", KIND_WEAPON, STAT_DAMAGE, 5, 8},
    [ITEM_MOSQUETE] = {"Mosquete", KIND_WEAPON, STAT_DAMAGE, 8, 12},
    [ITEM_CANHAO] = {"Canhão leve", KIND_WEAPON, STAT_DAMAGE, 12, 18},
    [ITEM_VARA] = {"Vara de bambu", KIND_TOOL, STAT_PRODUCTION, 5, 10},
    [ITEM_REDE] = {"Rede de pesca", KIND_TOOL, STAT_PRODUCTION, 12, 20},
    [ITEM_ARRASTO] = {"Rede de arrasto", KIND_TOOL, STAT_PRODUCTION, 20, 30},
    [ITEM_MOTOR] = {"Motor a vapor", KIND_BOOST, STAT_SPEED, 1, 2},
    [ITEM_CASCO] = {"Casco reforçado", KIND_BOOST, STAT_HP, 15, 30},
    [ITEM_AMULETO] = {"Amuleto de búzios", KIND_BOOST, STAT_LUCK, 5, 12},
    [ITEM_ISCA] = {"Isca encantada", KIND_BOOST, STAT_PRODUCTION, 4, 8},
    // Únicas: a faixa parece baixa, mas elas são sempre lendárias (x2,2).
    [ITEM_DENTE] = {"Dente do Megalodon", KIND_WEAPON, STAT_DAMAGE, 9, 11},
    [ITEM_TENTACULO] = {"Tentáculo do Kraken", KIND_WEAPON, STAT_DAMAGE, 7, 9},
    [ITEM_CAJADO] = {"Cajado de R'lyeh", KIND_WEAPON, STAT_DAMAGE, 6, 8},
};

const affix_t AFFIXES[AFFIX_COUNT] = {
    [AFFIX_VELOZ] = {"Veloz", STAT_SPEED, 1, 1},
    [AFFIX_ROBUSTO] = {"Robusto", STAT_HP, 5, 15},
    [AFFIX_AFIADO] = {"Afiado", STAT_DAMAGE, 2, 5},
    [AFFIX_PESCADOR] = {"do Pescador", STAT_PRODUCTION, 4, 10},
    [AFFIX_SORTE] = {"da Sorte", STAT_LUCK, 3, 8},
};

// Chance de cada raridade, em milésimos: 70%, 22%, 7%, 1%.
static const uint16_t RARITY_CHANCE[RARITY_COUNT] = {700, 220, 70, 10};
// Quanto a raridade multiplica os números (%).
static const uint16_t RARITY_SCALE[RARITY_COUNT] = {100, 130, 165, 220};

// Gerador do item: começa da semente e sempre dá a mesma sequência.
static uint32_t next(uint32_t *s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}

static int16_t roll(uint32_t *s, int16_t min, int16_t max, uint8_t rarity) {
    int v = min + (int)(next(s) % (uint32_t)(max - min + 1));
    return (int16_t)((v * RARITY_SCALE[rarity] + 50) / 100); // arredonda
}

item_t item_new(uint8_t base, uint32_t seed) {
    return (item_t){.seed = seed, .base = base};
}

item_info_t item_info(item_t item) {
    item_info_t info = {.base = item.base, .affix = {AFFIX_NONE, AFFIX_NONE, AFFIX_NONE}};
    if (item.base == ITEM_NONE || item.base >= ITEM_BASE_COUNT) return info;

    // Embaralha a semente antes (sementes parecidas dariam itens parecidos)
    // e nunca deixa zero, que travaria o xorshift.
    uint32_t s = item.seed * 0x9E3779B1u;
    s ^= s >> 16;
    s = s ? s : 1;

    uint32_t r = next(&s) % 1000;
    while (info.rarity < RARITY_LEGENDARY && r >= RARITY_CHANCE[info.rarity]) r -= RARITY_CHANCE[info.rarity++];
    if (item_is_unique(item.base)) info.rarity = RARITY_LEGENDARY; // arma de boss é sempre lendária

    const item_base_t *b = &ITEM_BASES[item.base];
    info.stats[b->stat] = roll(&s, b->min, b->max, info.rarity);

    // Comum: 0 ou 1 afixo. Raro: 1. Épico: 2. Lendário: 3. Nunca repete.
    int count = info.rarity == RARITY_COMMON ? (int)(next(&s) % 2) : info.rarity;
    for (int i = 0; i < count; i++) {
        uint8_t a;
        int repeated;
        do {
            a = (uint8_t)(next(&s) % AFFIX_COUNT);
            repeated = 0;
            for (int j = 0; j < i; j++) repeated |= info.affix[j] == a;
        } while (repeated);
        info.affix[i] = a;
        info.stats[AFFIXES[a].stat] += roll(&s, AFFIXES[a].min, AFFIXES[a].max, info.rarity);
    }
    return info;
}

item_info_t item_info_enchanted(item_t item, uint8_t level) {
    item_info_t info = item_info(item);
    if (level > ENCHANT_MAX) level = ENCHANT_MAX;
    for (int s = 0; level && s < STAT_COUNT; s++) {
        int v = info.stats[s];
        if (v <= 0) continue; // stat negativo ou zero não cresce
        int bonus = (v * ENCHANT_PCT * level + 50) / 100;
        info.stats[s] = (int16_t)(v + (bonus > level ? bonus : level));
    }
    return info;
}
