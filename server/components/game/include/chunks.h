// O mapa do servidor em pedaços (chunks) de 64x64 células. Cada chunk é
// gerado com world_tile_at() na primeira vez que alguém olha para ele e
// fica guardado numa tabela hash. Assim só existe na memória o que já foi
// visitado, e o mapa pode ser bem maior que a RAM.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "world.h"

#define CHUNK_SHIFT 6
#define CHUNK_CELLS (1 << CHUNK_SHIFT) // 64

void chunks_init(uint32_t seed); // apaga os chunks guardados e troca a semente
tile_t chunks_tile(int32_t x, int32_t y); // fora do mapa = montanha
size_t chunks_count(void);                // quantos chunks estão na memória
