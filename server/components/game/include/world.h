// Geração do oceano. Só usa inteiros, para dar exatamente o mesmo mapa
// no ESP32-S3 e no cliente WASM (client/src/world.odin é a mesma conta).
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define MAP_W 1024
#define MAP_H 1024

// Ilha-porto inicial no centro do mapa.
#define PORT_X (MAP_W / 2)
#define PORT_Y (MAP_H / 2)
#define PORT_RADIUS 7
#define DOCK_X PORT_X
#define DOCK_Y (PORT_Y + PORT_RADIUS)

// Regiões: o mapa é uma grade de centros a cada REGION_CELLS células, e cada
// centro tem um bioma. A região de uma célula é a do centro mais perto, então
// o porto (no meio do mapa) fica bem no centro da região dele.
#define REGION_CELLS 128
#define REGIONS_W (MAP_W / REGION_CELLS + 1)
#define REGIONS_H (MAP_H / REGION_CELLS + 1)
#define PORT_RX (PORT_X / REGION_CELLS)
#define PORT_RY (PORT_Y / REGION_CELLS)

typedef enum {
    BIOME_BAY = 0, // Baía: rasa e calma, poucas ilhas
    BIOME_MANGROVE, // Mangue: muita água rasa e costa recortada
    BIOME_ISLANDS,  // Arquipélago: muitas ilhas
    BIOME_OPEN,     // Mar aberto: quase só água funda
    BIOME_REEF,     // Recife: raso com pedras
    BIOME_ABYSS,    // Abismo: muito fundo
    BIOME_COUNT,
} biome_t;

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
biome_t world_region_biome(int32_t rx, int32_t ry, uint32_t seed);
int32_t world_region_of(int32_t v); // região (rx ou ry) de uma coordenada x ou y
tile_t world_tile_at(int32_t x, int32_t y, uint32_t seed);
void world_generate(uint8_t *tiles, uint32_t seed); // tiles: MAP_W * MAP_H bytes
uint32_t world_checksum(const uint8_t *tiles);      // FNV-1a, para comparar com o cliente

static inline bool tile_is_water(tile_t t) { return t <= TILE_SHALLOW; }
static inline bool tile_is_sailable(tile_t t) { return t <= TILE_SHALLOW || t == TILE_DOCK; }
