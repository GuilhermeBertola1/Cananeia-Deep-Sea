// Desenha o mapa de uma semente numa imagem BMP, para ver como os biomas
// ficaram sem abrir o jogo. Uso: mapa [semente] [arquivo.bmp]
// (padrão: semente 1337, mapa.bmp). Linhas finas marcam as divisas das regiões.
#include <stdio.h>
#include <stdlib.h>
#include "world.h"

static const uint8_t TILE_RGB[][3] = {
    {12, 30, 70},    // fundo
    {25, 70, 140},   // água
    {60, 140, 190},  // raso
    {220, 200, 140}, // areia
    {80, 150, 60},   // grama
    {30, 90, 40},    // floresta
    {120, 110, 100}, // montanha
    {150, 90, 40},   // cais
};

static void put_u32(FILE *f, uint32_t v) { fputc(v & 255, f), fputc(v >> 8 & 255, f), fputc(v >> 16 & 255, f), fputc(v >> 24, f); }
static void put_u16(FILE *f, uint16_t v) { fputc(v & 255, f), fputc(v >> 8, f); }

int main(int argc, char **argv) {
    uint32_t seed = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 10) : 1337;
    const char *path = argc > 2 ? argv[2] : "mapa.bmp";
    static uint8_t tiles[MAP_W * MAP_H];
    world_generate(tiles, seed);

    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return 1; }
    uint32_t row = MAP_W * 3; // MAP_W é múltiplo de 4, então a linha não precisa de enchimento
    fputs("BM", f);
    put_u32(f, 54 + row * MAP_H), put_u32(f, 0), put_u32(f, 54);
    put_u32(f, 40), put_u32(f, MAP_W), put_u32(f, MAP_H), put_u16(f, 1), put_u16(f, 24);
    put_u32(f, 0), put_u32(f, row * MAP_H), put_u32(f, 2835), put_u32(f, 2835), put_u32(f, 0), put_u32(f, 0);
    for (int y = MAP_H - 1; y >= 0; y--) { // o BMP guarda de baixo para cima
        for (int x = 0; x < MAP_W; x++) {
            const uint8_t *c = TILE_RGB[tiles[y * MAP_W + x]];
            int border = world_region_of(x) != world_region_of(x + 1) || world_region_of(y) != world_region_of(y + 1);
            int k = border ? 2 : 1; // divisa: cor mais escura
            fputc(c[2] / k, f), fputc(c[1] / k, f), fputc(c[0] / k, f);
        }
    }
    fclose(f);

    printf("semente %u: %dx%d, checksum %08x\n", seed, MAP_W, MAP_H, world_checksum(tiles));
    static const char *NAMES[BIOME_COUNT] = {"Baia", "Mangue", "Arquipelago", "Mar aberto", "Recife", "Abismo"};
    for (int ry = 0; ry < REGIONS_H; ry++) {
        for (int rx = 0; rx < REGIONS_W; rx++) printf("%-12s", NAMES[world_region_biome(rx, ry, seed)]);
        printf("\n");
    }
    printf("imagem salva em %s\n", path);
    return 0;
}
