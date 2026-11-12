// Testa se o mar é navegável em 200 sementes. O mundo inteiro não cabe na
// memória, então o teste olha duas janelas de 1024 x 1024: uma em volta do
// porto (a busca sai do cais) e uma no mar distante (a busca sai do centro de
// uma região). Em cada janela, todo centro de região tem que ser alcançável,
// e quase toda a água também.
// Uso: test_mapa   (sai com 1 se alguma semente falhar)
#include <stdio.h>
#include <stdlib.h>
#include "world.h"

#define SEEDS 200
#define MIN_WATER_PCT 97 // lagos pequenos dentro de ilhas não fazem mal
#define WIN 1024
// Centro da janela distante: região a 20 anéis do porto, já no mar do abismo.
#define FAR_X (PORT_X + 20 * REGION_CELLS)
#define FAR_Y (PORT_Y - 12 * REGION_CELLS)

static uint8_t tiles[WIN * WIN];
static uint8_t seen[WIN * WIN];
static int32_t queue[WIN * WIN];
static int x0, y0; // canto da janela no mundo

static void load_window(int cx, int cy, uint32_t seed) {
    x0 = cx - WIN / 2, y0 = cy - WIN / 2;
    for (int y = 0; y < WIN; y++)
        for (int x = 0; x < WIN; x++) tiles[y * WIN + x] = (uint8_t)world_tile_at(x0 + x, y0 + y, seed);
}

// Busca em largura a partir de (sx, sy) do mundo; devolve quantas células alcançou.
static int flood_from(int sx, int sy) {
    int head = 0, tail = 0, n = 0;
    for (int i = 0; i < WIN * WIN; i++) seen[i] = 0;
    int start = (sy - y0) * WIN + (sx - x0);
    if (!tile_is_sailable((tile_t)tiles[start])) return 0;
    queue[tail++] = start;
    seen[start] = 1;
    while (head < tail) {
        int i = queue[head++], x = i % WIN, y = i / WIN;
        n++;
        static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (nx < 0 || ny < 0 || nx >= WIN || ny >= WIN) continue;
            int j = ny * WIN + nx;
            if (seen[j] || !tile_is_sailable((tile_t)tiles[j])) continue;
            seen[j] = 1;
            queue[tail++] = j;
        }
    }
    return n;
}

// Confere a janela já carregada e buscada: devolve 1 se passou.
static int check_window(const char *name, uint32_t seed, int reached) {
    int water = 0, centers_bad = 0;
    for (int i = 0; i < WIN * WIN; i++) water += tile_is_sailable((tile_t)tiles[i]);
    for (int ry = world_region_of(y0); ry <= world_region_of(y0 + WIN - 1); ry++)
        for (int rx = world_region_of(x0); rx <= world_region_of(x0 + WIN - 1); rx++) {
            int x = rx * REGION_CELLS - x0, y = ry * REGION_CELLS - y0;
            // Centro na beirada da janela: o canal dele pode sair da janela.
            if (x <= 0 || y <= 0 || x >= WIN - 1 || y >= WIN - 1) continue;
            if (rx * REGION_CELLS == PORT_X && ry * REGION_CELLS == PORT_Y) continue; // o centro do porto é a ilha
            centers_bad += !seen[y * WIN + x];
        }
    int pct = water ? (int)((int64_t)reached * 100 / water) : 0;
    if (centers_bad || pct < MIN_WATER_PCT) {
        printf("FALHOU semente %u (%s): %d centros sem caminho, %d%% da água alcançável\n", seed, name, centers_bad, pct);
        return 0;
    }
    if (seed == 1337) printf("semente 1337 (%s): %d%% da água alcançável\n", name, pct);
    return 1;
}

int main(void) {
    int failed = 0;
    for (uint32_t s = 0; s <= SEEDS; s++) {
        uint32_t seed = s == SEEDS ? 1337 : s + 1;
        int ok = 1;
        load_window(PORT_X, PORT_Y, seed);
        ok &= check_window("porto", seed, flood_from(DOCK_X, DOCK_Y));
        load_window(FAR_X, FAR_Y, seed);
        ok &= check_window("mar distante", seed, flood_from(FAR_X, FAR_Y));
        failed += !ok;
    }
    printf("%s: %d de %d sementes falharam\n", failed ? "FALHOU" : "OK", failed, SEEDS + 1);
    return failed ? 1 : 0;
}
