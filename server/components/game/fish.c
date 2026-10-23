#include "fish.h"

const species_t SPECIES[FISH_COUNT] = {
    [FISH_SARDINHA]   = {"Sardinha", 100, 60, TILE_SHALLOW},
    [FISH_CAMARAO]    = {"Camarão", 200, 35, TILE_SHALLOW},
    [FISH_CARANGUEJO] = {"Caranguejo", 400, 5, TILE_SHALLOW},
    [FISH_ROBALO]     = {"Robalo", 400, 50, TILE_WATER},
    [FISH_ATUM]       = {"Atum", 600, 40, TILE_WATER},
    [FISH_TARTARUGA]  = {"Tartaruga", 1200, 10, TILE_WATER},
    [FISH_POLVO]      = {"Polvo", 1000, 50, TILE_DEEP},
    [FISH_ESPADA]     = {"Peixe-espada", 1500, 40, TILE_DEEP},
    [FISH_LULA]       = {"Lula-gigante", 4000, 10, TILE_DEEP},
};

uint8_t fish_roll(tile_t tile, uint32_t r) {
    uint32_t total = 0;
    for (int i = 0; i < FISH_COUNT; i++)
        if (SPECIES[i].biome == tile) total += SPECIES[i].weight;
    if (total == 0) return FISH_NONE;

    uint32_t pick = r % total;
    for (int i = 0; i < FISH_COUNT; i++) {
        if (SPECIES[i].biome != tile) continue;
        if (pick < SPECIES[i].weight) return (uint8_t)i;
        pick -= SPECIES[i].weight;
    }
    return FISH_NONE;
}
