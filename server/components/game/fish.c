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

// Quanto cada bioma do mapa puxa cada espécie, em % do peso normal (0 = 100%,
// o peso de sempre). O tile ainda decide quem pode sair (raso, água, fundo);
// o bioma só muda as chances: no Mangue sai mais camarão e caranguejo, no
// Abismo sai mais lula-gigante.
static const uint16_t BIOME_PCT[BIOME_COUNT][FISH_COUNT] = {
    [BIOME_BAY] = {[FISH_SARDINHA] = 150},
    [BIOME_MANGROVE] = {[FISH_CAMARAO] = 200, [FISH_CARANGUEJO] = 300, [FISH_ROBALO] = 150},
    [BIOME_ISLANDS] = {[FISH_ROBALO] = 130, [FISH_TARTARUGA] = 150},
    [BIOME_OPEN] = {[FISH_ATUM] = 200, [FISH_ESPADA] = 130},
    [BIOME_REEF] = {[FISH_CARANGUEJO] = 150, [FISH_TARTARUGA] = 200, [FISH_POLVO] = 200},
    [BIOME_ABYSS] = {[FISH_POLVO] = 50, [FISH_ESPADA] = 150, [FISH_LULA] = 300},
};

uint16_t fish_biome_pct(int species, biome_t biome) {
    uint16_t pct = BIOME_PCT[biome][species];
    return pct ? pct : 100;
}

static uint32_t weight_in(int species, biome_t biome) {
    return (uint32_t)SPECIES[species].weight * fish_biome_pct(species, biome) / 100;
}

uint8_t fish_roll(tile_t tile, biome_t biome, uint32_t r) {
    uint32_t total = 0;
    for (int i = 0; i < FISH_COUNT; i++)
        if (SPECIES[i].biome == tile) total += weight_in(i, biome);
    if (total == 0) return FISH_NONE;

    uint32_t pick = r % total;
    for (int i = 0; i < FISH_COUNT; i++) {
        if (SPECIES[i].biome != tile) continue;
        uint32_t w = weight_in(i, biome);
        if (pick < w) return (uint8_t)i;
        pick -= w;
    }
    return FISH_NONE;
}
