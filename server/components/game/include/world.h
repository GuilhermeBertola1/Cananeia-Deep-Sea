// Geração do oceano. Só usa inteiros, para dar exatamente o mesmo mapa
// no ESP32-S3 e no cliente WASM (client/src/world.odin é a mesma conta).
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define MAP_W 512
#define MAP_H 512

// Ilha-porto inicial no centro do mapa.
#define PORT_X (MAP_W / 2)
#define PORT_Y (MAP_H / 2)
#define PORT_RADIUS 7
#define DOCK_X PORT_X
#define DOCK_Y (PORT_Y + PORT_RADIUS)

typedef enum {
    TILE_DEEP = 0,
    TILE_WATER,
    TILE_SHALLOW,
    TILE_SAND,
    TILE_GRASS,
    TILE_FOREST,
    TILE_MOUNTAIN,
    TILE_DOCK,
} tile_t;

uint32_t world_hash(int32_t x, int32_t y, uint32_t seed);
tile_t world_tile_at(int32_t x, int32_t y, uint32_t seed);
void world_generate(uint8_t *tiles, uint32_t seed); // tiles: MAP_W * MAP_H bytes
uint32_t world_checksum(const uint8_t *tiles);      // FNV-1a, para comparar com o cliente

static inline bool tile_is_water(tile_t t) { return t <= TILE_SHALLOW; }
static inline bool tile_is_sailable(tile_t t) { return t <= TILE_SHALLOW || t == TILE_DOCK; }
