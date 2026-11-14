#include "chunks.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    int32_t cx, cy; // posição do chunk (célula / 64)
    uint8_t tiles[CHUNK_CELLS * CHUNK_CELLS];
} chunk_t;

// Tabela hash com endereçamento aberto: um vetor de ponteiros, e se a casa
// do chunk está ocupada por outro, tenta a próxima (sondagem linear). Cresce
// para o dobro quando passa da metade cheia, para a busca continuar curta.
static struct {
    uint32_t seed;
    chunk_t **slots;
    size_t cap; // sempre potência de 2
    size_t count;
    chunk_t *last; // o último chunk usado: quase sempre é o mesmo da próxima vez
} c;

static size_t slot_of(int32_t cx, int32_t cy) { return world_hash(cx, cy, 0xC4C4E) & (c.cap - 1); }

static void put(chunk_t *ch) {
    size_t i = slot_of(ch->cx, ch->cy);
    while (c.slots[i]) i = (i + 1) & (c.cap - 1);
    c.slots[i] = ch;
}

static void grow(void) {
    chunk_t **old = c.slots;
    size_t old_cap = c.cap;
    c.cap = old_cap ? old_cap * 2 : 1024;
    c.slots = calloc(c.cap, sizeof *c.slots);
    if (!c.slots) abort(); // sem memória: não tem como continuar
    for (size_t i = 0; i < old_cap; i++)
        if (old[i]) put(old[i]);
    free(old);
}

static chunk_t *find(int32_t cx, int32_t cy) {
    for (size_t i = slot_of(cx, cy);; i = (i + 1) & (c.cap - 1)) {
        chunk_t *ch = c.slots[i];
        if (!ch) return NULL;
        if (ch->cx == cx && ch->cy == cy) return ch;
    }
}

static chunk_t *create(int32_t cx, int32_t cy) {
    if ((c.count + 1) * 2 > c.cap) grow();
    chunk_t *ch = malloc(sizeof *ch);
    if (!ch) abort();
    ch->cx = cx;
    ch->cy = cy;
    for (int y = 0; y < CHUNK_CELLS; y++)
        for (int x = 0; x < CHUNK_CELLS; x++)
            ch->tiles[y * CHUNK_CELLS + x] =
                (uint8_t)world_tile_at(cx * CHUNK_CELLS + x, cy * CHUNK_CELLS + y, c.seed);
    put(ch);
    c.count++;
    return ch;
}

void chunks_init(uint32_t seed) {
    for (size_t i = 0; i < c.cap; i++) free(c.slots[i]);
    free(c.slots);
    memset(&c, 0, sizeof c);
    c.seed = seed;
    grow();
}

tile_t chunks_tile(int32_t x, int32_t y) {
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return TILE_MOUNTAIN;
    int32_t cx = x >> CHUNK_SHIFT, cy = y >> CHUNK_SHIFT;
    chunk_t *ch = c.last;
    if (!ch || ch->cx != cx || ch->cy != cy) {
        ch = find(cx, cy);
        if (!ch) ch = create(cx, cy);
        c.last = ch;
    }
    return (tile_t)ch->tiles[(y & (CHUNK_CELLS - 1)) * CHUNK_CELLS + (x & (CHUNK_CELLS - 1))];
}

size_t chunks_count(void) { return c.count; }
