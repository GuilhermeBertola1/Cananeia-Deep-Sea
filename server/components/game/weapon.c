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

const weapon_t *weapon_of(uint8_t item_base) {
    if (item_base >= ITEM_BASE_COUNT || ITEM_BASES[item_base].kind != KIND_WEAPON) return 0;
    return &WEAPONS[item_base];
}