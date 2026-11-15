// Caminho pelo mar: busca em largura (BFS) numa janela do mapa em volta da
// saída e da chegada. Os pescadores NPC usam para ir e voltar sem ficar
// batendo na costa.
#pragma once
#include <stdint.h>

// A janela vai 32 células além da saída e da chegada; se não achar, 96 e
// depois 192. E nunca passa de 448 x 448 células.
#define PATH_WINDOW_MAX 448
#define PATH_MAX_LEN 768        // células no caminho, no máximo

typedef struct {
    int32_t x[PATH_MAX_LEN], y[PATH_MAX_LEN];
    int len;  // 0 = sem caminho
    int next; // próxima célula a alcançar
} path_t;

// Acha o caminho de (sx, sy) até (tx, ty) só por células navegáveis, longe
// da costa quando dá (uma célula de folga). O caminho não inclui a saída.
// Devolve 1 se achou; 0 se não tem caminho em nenhuma janela, se a janela
// seria grande demais ou se o caminho é comprido demais.
int path_find(path_t *p, int sx, int sy, int tx, int ty);
