// Testa se o mar do mapa é todo navegável a partir do cais em 200 sementes:
// todo centro de região tem que ser alcançável, e quase toda a água também.
// Uso: test_mapa   (sai com 1 se alguma semente falhar)
#include <stdio.h>
#include <stdlib.h>
#include "world.h"

#define SEEDS 200
#define MIN_WATER_PCT 97 // lagos pequenos dentro de ilhas não fazem mal

static uint8_t tiles[MAP_W * MAP_H];
static uint8_t seen[MAP_W * MAP_H];
static int32_t queue[MAP_W * MAP_H];

// Busca em largura a partir do cais; devolve quantas células alcançou.
static int flood_from_dock(void) {
    int head = 0, tail = 0, n = 0;
    for (int i = 0; i < MAP_W * MAP_H; i++) seen[i] = 0;
    queue[tail++] = DOCK_Y * MAP_W + DOCK_X;
    seen[DOCK_Y * MAP_W + DOCK_X] = 1;
    while (head < tail) {
        int i = queue[head++], x = i % MAP_W, y = i / MAP_W;
        n++;
        static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (nx < 0 || ny < 0 || nx >= MAP_W || ny >= MAP_H) continue;
            int j = ny * MAP_W + nx;
            if (seen[j] || !tile_is_sailable((tile_t)tiles[j])) continue;
            seen[j] = 1;
            queue[tail++] = j;
        }
    }
    return n;
}

int main(void) {
    int failed = 0;
    for (uint32_t s = 0; s <= SEEDS; s++) {
        uint32_t seed = s == SEEDS ? 1337 : s + 1;
        world_generate(tiles, seed);
        int reached = flood_from_dock(), water = 0, centers_bad = 0;
        for (int i = 0; i < MAP_W * MAP_H; i++) water += tile_is_sailable((tile_t)tiles[i]);
        for (int ry = 0; ry < REGIONS_H; ry++)
            for (int rx = 0; rx < REGIONS_W; rx++) {
                int x = rx * REGION_CELLS, y = ry * REGION_CELLS;
                if (x >= MAP_W || y >= MAP_H) continue;
                if (x == PORT_X && y == PORT_Y) continue; // o centro do porto é a ilha
                centers_bad += !seen[y * MAP_W + x];
            }
        int pct = (int)((int64_t)reached * 100 / water);
        if (centers_bad || pct < MIN_WATER_PCT) {
            printf("FALHOU semente %u: %d centros sem caminho, %d%% da água alcançável\n", seed, centers_bad, pct);
            failed++;
        } else if (seed == 1337) {
            printf("semente 1337: %d%% da água alcançável, checksum %08x\n", pct, world_checksum(tiles));
        }
    }
    printf("%s: %d de %d sementes falharam\n", failed ? "FALHOU" : "OK", failed, SEEDS + 1);
    return failed ? 1 : 0;
}
