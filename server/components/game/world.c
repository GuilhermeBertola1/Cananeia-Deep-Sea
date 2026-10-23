#include "world.h"

// Todos os valores de ruído estão em ponto fixo 16 bits: 0..65535 = 0.0..1.0.
#define ONE 65536

uint32_t world_hash(int32_t x, int32_t y, uint32_t seed) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + seed * 2147483647u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// Curva suave 3t² - 2t³, em ponto fixo.
static int32_t smooth(int32_t t) {
    int64_t t2 = (int64_t)t * t / ONE;
    return (int32_t)(t2 * (3 * ONE - 2 * (int64_t)t) / ONE);
}

// Ruído de valor numa grade de `cell` células (potência de 2).
static int32_t value_noise(int32_t x, int32_t y, int32_t cell, uint32_t seed) {
    int32_t ix = x / cell, iy = y / cell;
    int32_t fx = smooth((x % cell) * ONE / cell);
    int32_t fy = smooth((y % cell) * ONE / cell);
    int32_t a = world_hash(ix, iy, seed) >> 16;
    int32_t b = world_hash(ix + 1, iy, seed) >> 16;
    int32_t c = world_hash(ix, iy + 1, seed) >> 16;
    int32_t d = world_hash(ix + 1, iy + 1, seed) >> 16;
    int32_t top = a + (int32_t)((int64_t)(b - a) * fx / ONE);
    int32_t bottom = c + (int32_t)((int64_t)(d - c) * fx / ONE);
    return top + (int32_t)((int64_t)(bottom - top) * fy / ONE);
}

// Soma de 5 oitavas: células de 64, 32, 16, 8 e 4, com pesos 16, 8, 4, 2, 1.
static int32_t fbm(int32_t x, int32_t y, uint32_t seed) {
    int32_t sum = 0;
    for (int o = 0; o < 5; o++) {
        sum += value_noise(x, y, 64 >> o, seed + o * 101) * (16 >> o);
    }
    return sum / 31;
}

static int32_t isqrt(int32_t n) {
    int32_t r = 0;
    while ((r + 1) * (r + 1) <= n) r++;
    return r;
}

tile_t world_tile_at(int32_t x, int32_t y, uint32_t seed) {
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return TILE_MOUNTAIN;
    if (x == DOCK_X && y == DOCK_Y) return TILE_DOCK;

    int32_t dx = x - PORT_X, dy = y - PORT_Y;
    int32_t d2 = dx * dx + dy * dy;
    // Ilha-porto e águas calmas em volta dela.
    if (d2 <= PORT_RADIUS * PORT_RADIUS) return d2 > 30 ? TILE_SAND : d2 < 4 ? TILE_FOREST : TILE_GRASS;
    if (d2 <= 10 * 10) return TILE_SHALLOW;
    if (d2 <= 21 * 21) return TILE_WATER;

    // Quanto mais longe do centro, mais fundo fica o mar.
    int32_t e = fbm(x, y, seed) - isqrt(d2) * 14;
    if (e < 26214) return TILE_DEEP;
    if (e < 36045) return TILE_WATER;
    if (e < 39322) return TILE_SHALLOW;
    if (e < 40632) return TILE_SAND;
    if (e < 43909) return TILE_GRASS;
    if (e < 47186) return TILE_FOREST;
    return TILE_MOUNTAIN;
}

void world_generate(uint8_t *tiles, uint32_t seed) {
    for (int32_t y = 0; y < MAP_H; y++)
        for (int32_t x = 0; x < MAP_W; x++)
            tiles[y * MAP_W + x] = (uint8_t)world_tile_at(x, y, seed);
}

uint32_t world_checksum(const uint8_t *tiles) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < MAP_W * MAP_H; i++) h = (h ^ tiles[i]) * 16777619u;
    return h;
}
