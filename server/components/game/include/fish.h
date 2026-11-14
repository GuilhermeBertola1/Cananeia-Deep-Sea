// Espécies de peixe. A mesma tabela existe em client/src/fish.odin;
// se mudar aqui, mude lá também (a ordem é o id enviado na rede).
#pragma once
#include <stdint.h>
#include "world.h"

typedef enum {
    FISH_SARDINHA = 0,
    FISH_CAMARAO,
    FISH_CARANGUEJO,
    FISH_ROBALO,
    FISH_ATUM,
    FISH_TARTARUGA,
    FISH_POLVO,
    FISH_ESPADA,
    FISH_LULA,
    FISH_COUNT,
} fish_t;

#define FISH_NONE 0xFF

typedef struct {
    const char *name;
    uint16_t price_cents; // quanto o lojista paga sem pressão de mercado
    uint8_t weight;       // chance relativa dentro do bioma
    uint8_t biome;        // tile_t onde a espécie vive
} species_t;

extern const species_t SPECIES[FISH_COUNT];

// Sorteia uma espécie que vive no tile, com as chances do bioma da região;
// `r` é um número aleatório qualquer. Devolve FISH_NONE se o tile não tem
// peixe (terra).
uint8_t fish_roll(tile_t tile, biome_t biome, uint32_t r);
