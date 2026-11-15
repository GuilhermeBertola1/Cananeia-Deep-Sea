// Bandido multibraço ε-guloso. Ver brain.h.
#include "brain.h"
#include <string.h>

// O braço que está há mais tempo sem ser jogado.
static int brain_oldest(const brain_t *b) {
    int old = 0;
    for (int i = 1; i < b->arms; i++)
        if (b->last[i] < b->last[old]) old = i;
    return old;
}

static uint32_t brain_rnd(brain_t *b) {
    b->rng ^= b->rng << 13, b->rng ^= b->rng >> 17, b->rng ^= b->rng << 5;
    return b->rng;
}

void brain_init(brain_t *b, int arms, uint32_t seed) {
    memset(b, 0, sizeof *b);
    b->arms = (uint8_t)(arms < 1 ? 1 : arms > BRAIN_MAX_ARMS ? BRAIN_MAX_ARMS : arms);
    b->epsilon = BRAIN_EPSILON_START;
    b->rng = seed ? seed : 0x2545F491u; // xorshift não pode começar em zero
}

int brain_best(const brain_t *b) {
    int best = 0;
    for (int i = 1; i < b->arms; i++)
        if (b->value[i] > b->value[best]) best = i;
    return best;
}

int brain_choose(brain_t *b) {
    b->explored = 0;
    b->round++;
    // 1) Braço que nunca foi jogado: sem nenhum número, não dá para comparar.
    for (int i = 0; i < b->arms; i++) {
        if (!b->tries[i]) {
            b->current = (uint8_t)i;
            b->last[i] = b->round;
            return i;
        }
    }
    // 2) Explorar, com chance ε: confere o braço mais esquecido. Sortear um
    //    qualquer também funciona, mas pode passar muito tempo sem olhar
    //    justo o que melhorou. 3) Senão, aproveitar o melhor.
    if (brain_rnd(b) % 1000 < b->epsilon) {
        b->current = (uint8_t)brain_oldest(b);
        b->explored = 1;
    } else {
        b->current = (uint8_t)brain_best(b);
    }
    b->last[b->current] = b->round;
    // Quanto mais ele joga, menos precisa explorar (mas nunca para de vez).
    b->epsilon = (uint16_t)(b->epsilon * BRAIN_EPSILON_DECAY_PCT / 100);
    if (b->epsilon < BRAIN_EPSILON_MIN) b->epsilon = BRAIN_EPSILON_MIN;
    return b->current;
}

void brain_reward(brain_t *b, int32_t reward) {
    int i = b->current;
    if (b->tries[i] < UINT16_MAX) b->tries[i]++;
    if (b->tries[i] == 1) {
        b->value[i] = reward; // primeira vez: a média é o próprio resultado
        return;
    }
    // A média anda RATE% do caminho até o resultado novo:
    //   média = média + 0,3 × (resultado − média)
    // Resultados recentes pesam mais que os antigos.
    b->value[i] += (int32_t)((int64_t)(reward - b->value[i]) * BRAIN_RATE_PCT / 100);
}
