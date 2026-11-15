// Busca em largura. Ver path.h.
//
// A ideia: a partir da saída, visita todas as células de água a 1 passo, depois
// todas a 2 passos, e assim por diante (uma fila). Cada célula guarda de que
// direção veio. Quando chega no destino, é só voltar pelas direções até a
// saída: esse é o caminho mais curto em número de passos.
#include "path.h"
#include <stdlib.h>
#include "chunks.h"
#include "world.h"

static const int8_t DX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
static const int8_t DY[8] = {0, 1, 1, 1, 0, -1, -1, -1};

// Memória da busca, reaproveitada entre chamadas (alocada na primeira).
static uint8_t *from;  // 0 = não visitada; senão 1 + direção pela qual chegou
static int32_t *queue; // índices de células na janela

static int sailable(int x, int y) { return tile_is_sailable(chunks_tile(x, y)); }

// Água com folga: as 8 vizinhas também são navegáveis. O leme demora para
// virar, então um caminho colado na costa faz o barco bater nas curvas.
static int roomy(int x, int y) {
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (!sailable(x + dx, y + dy)) return 0;
    return 1;
}

static int cheb_dist(int ax, int ay, int bx, int by) {
    int dx = ax > bx ? ax - bx : bx - ax, dy = ay > by ? ay - by : by - ay;
    return dx > dy ? dx : dy;
}

// Uma busca. Com `need_room`, só passa por água com folga, menos perto da
// saída e da chegada (o cais é colado na ilha).
static int search(path_t *p, int sx, int sy, int tx, int ty, int margin, int need_room) {
    p->len = p->next = 0;
    if (!from) {
        from = malloc((size_t)PATH_WINDOW_MAX * PATH_WINDOW_MAX);
        queue = malloc(sizeof(int32_t) * PATH_WINDOW_MAX * PATH_WINDOW_MAX);
        if (!from || !queue) return 0;
    }
    int x0 = (sx < tx ? sx : tx) - margin, y0 = (sy < ty ? sy : ty) - margin;
    int w = (sx > tx ? sx - tx : tx - sx) + 2 * margin + 1;
    int h = (sy > ty ? sy - ty : ty - sy) + 2 * margin + 1;
    if (w > PATH_WINDOW_MAX || h > PATH_WINDOW_MAX) return 0;
    if (!sailable(tx, ty)) return 0;
    for (int i = 0; i < w * h; i++) from[i] = 0;

    int start = (sy - y0) * w + (sx - x0), goal = (ty - y0) * w + (tx - x0);
    int head = 0, tail = 0;
    queue[tail++] = start;
    from[start] = 9; // a saída: veio de lugar nenhum
    while (head < tail && !from[goal]) {
        int c = queue[head++], cx = c % w, cy = c / w;
        for (int d = 0; d < 8; d++) {
            int nx = cx + DX[d], ny = cy + DY[d];
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            int n = ny * w + nx;
            if (from[n] || !sailable(x0 + nx, y0 + ny)) continue;
            if (need_room && cheb_dist(x0 + nx, y0 + ny, sx, sy) > 3 && cheb_dist(x0 + nx, y0 + ny, tx, ty) > 3 &&
                !roomy(x0 + nx, y0 + ny))
                continue;
            // Na diagonal, as duas células do lado também têm que ser água:
            // assim o caminho não corta a quina de uma ilha.
            if (DX[d] && DY[d] && (!sailable(x0 + nx, y0 + cy) || !sailable(x0 + cx, y0 + ny))) continue;
            from[n] = (uint8_t)(1 + d);
            queue[tail++] = n;
        }
    }
    if (!from[goal]) return 0;

    // Conta os passos voltando do destino, e depois escreve do fim para o começo.
    int len = 0;
    for (int c = goal; c != start; len++) {
        int d = from[c] - 1;
        c = (c / w - DY[d]) * w + (c % w - DX[d]);
    }
    if (len > PATH_MAX_LEN) return 0;
    int i = len;
    for (int c = goal; c != start;) {
        i--;
        p->x[i] = x0 + c % w;
        p->y[i] = y0 + c / w;
        int d = from[c] - 1;
        c = (c / w - DY[d]) * w + (c % w - DX[d]);
    }
    p->len = len;
    return 1;
}

int path_find(path_t *p, int sx, int sy, int tx, int ty) {
    // Janela pequena primeiro (rápido); se a ilha no meio for grande, janelas
    // maiores. Em cada uma: pelo meio da água e, se não der, por onde der.
    static const int MARGINS[] = {32, 96, 192};
    for (int i = 0; i < 3; i++)
        if (search(p, sx, sy, tx, ty, MARGINS[i], 1) || search(p, sx, sy, tx, ty, MARGINS[i], 0)) return 1;
    return 0;
}
