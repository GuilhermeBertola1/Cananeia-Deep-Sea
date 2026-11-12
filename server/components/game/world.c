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

// ---------------------------------------------------------------- regiões

// Cada bioma muda duas coisas no ruído:
//   bias: soma no valor médio (mais alto = mais terra, mais baixo = mais fundo)
//   amp:  quanto o ruído varia em volta da média, em % (mais = ilhas mais altas)
typedef struct {
    int32_t bias;
    int32_t amp;
} biome_params_t;

static const biome_params_t BIOME_PARAMS[BIOME_COUNT] = {
    [BIOME_BAY] = {2000, 70},
    [BIOME_MANGROVE] = {5500, 50},
    [BIOME_ISLANDS] = {3000, 150},
    [BIOME_OPEN] = {-5000, 60},
    [BIOME_REEF] = {4000, 100},
    [BIOME_ABYSS] = {-12000, 40},
};

// Quais biomas podem sair em cada anel em volta do porto (anel 0 = a região
// do porto, anel 1 = as 8 vizinhas...). O último vale para os anéis de fora.
#define RINGS 4
static const uint8_t RING_BIOMES[RINGS][4] = {
    {BIOME_BAY, BIOME_BAY, BIOME_BAY, BIOME_BAY},
    {BIOME_BAY, BIOME_MANGROVE, BIOME_ISLANDS, BIOME_ISLANDS},
    {BIOME_MANGROVE, BIOME_ISLANDS, BIOME_OPEN, BIOME_REEF},
    {BIOME_OPEN, BIOME_REEF, BIOME_ABYSS, BIOME_ABYSS},
};

static int32_t iabs(int32_t v) { return v < 0 ? -v : v; }

int32_t world_region_of(int32_t v) { return (v + REGION_CELLS / 2) / REGION_CELLS; }

biome_t world_region_biome(int32_t rx, int32_t ry, uint32_t seed) {
    int32_t ring = iabs(rx - PORT_RX) > iabs(ry - PORT_RY) ? iabs(rx - PORT_RX) : iabs(ry - PORT_RY);
    if (ring >= RINGS) ring = RINGS - 1;
    uint32_t h = world_hash(rx, ry, seed ^ 0xB10E);
    return (biome_t)RING_BIOMES[ring][h % 4];
}

// Anda a fração t (0..ONE) do caminho de a até b.
static int32_t mix(int32_t a, int32_t b, int32_t t) { return a + (int32_t)((int64_t)(b - a) * t / ONE); }

// Mistura os parâmetros dos 4 centros de região em volta de (x, y), com a
// mesma curva suave do ruído. Assim a divisa entre dois biomas é uma
// transição de umas 100 células, e não uma linha reta no mapa.
static biome_params_t blended_params(int32_t x, int32_t y, uint32_t seed) {
    int32_t rx = x / REGION_CELLS, ry = y / REGION_CELLS;
    int32_t fx = smooth((x % REGION_CELLS) * ONE / REGION_CELLS);
    int32_t fy = smooth((y % REGION_CELLS) * ONE / REGION_CELLS);
    biome_params_t a = BIOME_PARAMS[world_region_biome(rx, ry, seed)];
    biome_params_t b = BIOME_PARAMS[world_region_biome(rx + 1, ry, seed)];
    biome_params_t c = BIOME_PARAMS[world_region_biome(rx, ry + 1, seed)];
    biome_params_t d = BIOME_PARAMS[world_region_biome(rx + 1, ry + 1, seed)];
    biome_params_t out = {
        .bias = mix(mix(a.bias, b.bias, fx), mix(c.bias, d.bias, fx), fy),
        .amp = mix(mix(a.amp, b.amp, fx), mix(c.amp, d.amp, fx), fy),
    };
    return out;
}

// ---------------------------------------------------------------- canais

// Cada divisa entre duas regiões vizinhas tem um ponto de passagem, sorteado
// a até 32 células do meio da divisa. Um canal reto liga o centro de cada
// região aos pontos de passagem dos seus 4 lados, então sempre dá para ir de
// uma região para a vizinha. Cada canal fica dentro da própria região, e por
// isso uma célula só precisa olhar os 4 canais da região dela.
#define CHANNEL_JITTER 32
#define CHANNEL_WIDTH2 2 // distância² até a linha do canal: 3 células de largura

// Prende v dentro do mapa, longe 2 células da borda (as regiões da borda
// têm o centro em cima da borda, e o canal não pode sair do mapa).
static int32_t clamp_in(int32_t v, int32_t size) { return v < 2 ? 2 : v > size - 3 ? size - 3 : v; }

// Ponto de passagem do lado direito (side 0) ou de baixo (side 1) da região.
static void passage(int32_t rx, int32_t ry, int side, uint32_t seed, int32_t *px, int32_t *py) {
    uint32_t h = world_hash(rx * 2 + (side == 0), ry * 2 + (side == 1), seed ^ 0xCA7A1);
    int32_t jitter = (int32_t)(h % (2 * CHANNEL_JITTER + 1)) - CHANNEL_JITTER;
    if (side == 0) {
        *px = rx * REGION_CELLS + REGION_CELLS / 2;
        *py = clamp_in(ry * REGION_CELLS + jitter, MAP_H);
    } else {
        *px = clamp_in(rx * REGION_CELLS + jitter, MAP_W);
        *py = ry * REGION_CELLS + REGION_CELLS / 2;
    }
}

// Distância² de (x, y) até o segmento de (ax, ay) a (bx, by).
static int32_t dist2_segment(int32_t x, int32_t y, int32_t ax, int32_t ay, int32_t bx, int32_t by) {
    int32_t vx = bx - ax, vy = by - ay;
    int32_t len2 = vx * vx + vy * vy;
    int32_t t = (x - ax) * vx + (y - ay) * vy; // projeção, ainda multiplicada por len2
    if (t < 0) t = 0;
    if (t > len2) t = len2;
    int32_t cx = ax + (len2 ? vx * t / len2 : 0), cy = ay + (len2 ? vy * t / len2 : 0);
    return (x - cx) * (x - cx) + (y - cy) * (y - cy);
}

static bool on_channel(int32_t x, int32_t y, uint32_t seed) {
    int32_t rx = world_region_of(x), ry = world_region_of(y);
    int32_t cx = rx * REGION_CELLS, cy = ry * REGION_CELLS;
    int32_t px, py;
    // Direita e baixo são desta região; esquerda e cima são da vizinha.
    if (rx + 1 < REGIONS_W) {
        passage(rx, ry, 0, seed, &px, &py);
        if (dist2_segment(x, y, cx, cy, px, py) <= CHANNEL_WIDTH2) return true;
    }
    if (ry + 1 < REGIONS_H) {
        passage(rx, ry, 1, seed, &px, &py);
        if (dist2_segment(x, y, cx, cy, px, py) <= CHANNEL_WIDTH2) return true;
    }
    if (rx > 0) {
        passage(rx - 1, ry, 0, seed, &px, &py);
        if (dist2_segment(x, y, cx, cy, px, py) <= CHANNEL_WIDTH2) return true;
    }
    if (ry > 0) {
        passage(rx, ry - 1, 1, seed, &px, &py);
        if (dist2_segment(x, y, cx, cy, px, py) <= CHANNEL_WIDTH2) return true;
    }
    return false;
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

    // O bioma da região decide a média e a força do ruído.
    biome_params_t bp = blended_params(x, y, seed);
    int32_t e = ONE / 2 + (fbm(x, y, seed) - ONE / 2) * bp.amp / 100 + bp.bias;
    if (e < 26214) return TILE_DEEP;
    if (e < 36045) return TILE_WATER;
    if (e < 39322) return TILE_SHALLOW;
    if (on_channel(x, y, seed)) return TILE_SHALLOW; // terra no caminho: o canal corta como um rio
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
