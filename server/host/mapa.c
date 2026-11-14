// Desenha um pedaço do mapa de uma semente numa imagem BMP, para ver como os
// biomas ficaram sem abrir o jogo. O mundo tem 65536 x 65536 células, então a
// imagem é só uma janela dele.
// Uso: mapa [semente] [arquivo.bmp] [tamanho] [x y]
// (padrão: semente 1337, mapa.bmp, 1024 células, centro no porto).
// Linhas finas marcam as divisas das regiões.
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
    int size = argc > 3 ? atoi(argv[3]) : 1024;
    int cx = argc > 5 ? atoi(argv[4]) : PORT_X, cy = argc > 5 ? atoi(argv[5]) : PORT_Y;
    size = size < 4 ? 4 : size > 8192 ? 8192 : size / 4 * 4; // múltiplo de 4: a linha do BMP não precisa de enchimento
    int x0 = cx - size / 2, y0 = cy - size / 2;

    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return 1; }
    uint32_t row = (uint32_t)size * 3;
    fputs("BM", f);
    put_u32(f, 54 + row * size), put_u32(f, 0), put_u32(f, 54);
    put_u32(f, 40), put_u32(f, size), put_u32(f, size), put_u16(f, 1), put_u16(f, 24);
    put_u32(f, 0), put_u32(f, row * size), put_u32(f, 2835), put_u32(f, 2835), put_u32(f, 0), put_u32(f, 0);
    uint32_t h = 2166136261u;
    for (int y = y0 + size - 1; y >= y0; y--) { // o BMP guarda de baixo para cima
        for (int x = x0; x < x0 + size; x++) {
            tile_t t = world_tile_at(x, y, seed);
            h = (h ^ (uint8_t)t) * 16777619u;
            const uint8_t *c = TILE_RGB[t];
            int border = world_region_of(x) != world_region_of(x + 1) || world_region_of(y) != world_region_of(y + 1);
            int k = border ? 2 : 1; // divisa: cor mais escura
            fputc(c[2] / k, f), fputc(c[1] / k, f), fputc(c[0] / k, f);
        }
    }
    fclose(f);

    printf("semente %u: janela de %dx%d com centro em (%d, %d), checksum %08x\n", seed, size, size, cx, cy, h);
    if (size <= 2048) {
        static const char *NAMES[BIOME_COUNT] = {"Baia", "Mangue", "Arquipelago", "Mar aberto", "Recife", "Abismo"};
        for (int ry = world_region_of(y0); ry <= world_region_of(y0 + size - 1); ry++) {
            for (int rx = world_region_of(x0); rx <= world_region_of(x0 + size - 1); rx++)
                printf("%-12s", NAMES[world_region_biome(rx, ry, seed)]);
            printf("\n");
        }
    }
    printf("imagem salva em %s\n", path);
    return 0;
}
