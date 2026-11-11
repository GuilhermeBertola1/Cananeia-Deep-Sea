#include "weapon.h"
#include "items.h"

// Arpão: perto e rápido. Mosquete: longe e certeiro. Canhão: dano alto,
// recarga lenta e bala pesada (mais fácil de desviar).
// As únicas (dos bosses) têm um poder especial em `fx`.
static const weapon_t WEAPONS[ITEM_BASE_COUNT] = {
    [ITEM_ARPAO] = {.range = 8, .reload = 6, .speed = 2},
    [ITEM_MOSQUETE] = {.range = 14, .reload = 12, .speed = 3},
    [ITEM_CANHAO] = {.range = 18, .reload = 20, .speed = 2},
    // Armas únicas dos bosses.
    [ITEM_DENTE] = {.range = 10, .reload = 8, .speed = 3, .fx = FX_LEECH},
    [ITEM_TENTACULO] = {.range = 12, .reload = 10, .speed = 3, .fx = FX_GRAB},
    [ITEM_CAJADO] = {.range = 16, .reload = 15, .speed = 2, .fx = FX_SPREAD},
};

// O raro custa 10x (o canhão, 8x): comprar dois comuns e juntar no mecânico
// ($80) sai mais barato, mas dá trabalho e o resultado é sorteado.
const armory_offer_t ARMORY[ARMORY_OFFERS] = {
    {ITEM_ARPAO, RARITY_COMMON, 4000},    {ITEM_ARPAO, RARITY_RARE, 40000},     // $40 e $400
    {ITEM_MOSQUETE, RARITY_COMMON, 12000}, {ITEM_MOSQUETE, RARITY_RARE, 100000}, // $120 e $1.000
    {ITEM_CANHAO, RARITY_COMMON, 30000},  {ITEM_CANHAO, RARITY_RARE, 240000},   // $300 e $2.400
};

const weapon_t *weapon_of(uint8_t item_base) {
    if (item_base >= ITEM_BASE_COUNT || ITEM_BASES[item_base].kind != KIND_WEAPON) return 0;
    return &WEAPONS[item_base];
}