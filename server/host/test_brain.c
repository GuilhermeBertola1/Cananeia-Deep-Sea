// Teste do cérebro (brain.c): um NPC de mentira com 4 estratégias que pagam
// diferente, com sorte no meio. Ele não sabe qual é a melhor; tem que
// descobrir jogando. Depois de 200 viagens o mundo muda (a melhor fica ruim e
// a pior fica ótima) e ele tem que perceber e trocar.
//
// Roda com 100 sementes, porque sorte boa numa só não prova nada.
//
// Uso: test_brain        -> roda e diz se passou
//      test_brain -v     -> mostra viagem por viagem da primeira semente
#include <stdio.h>
#include <string.h>
#include "brain.h"

#define ARMS 4
#define SEEDS 100
#define TRIPS 200 // antes e depois da mudança
static const char *NAMES[ARMS] = {"perto do porto", "cacar cardume", "estoque cheio", "longe, na vila"};

// Quanto cada estratégia paga em média ($/min), antes e depois da mudança.
static const int PAY_BEFORE[ARMS] = {40, 55, 45, 30};
static const int PAY_AFTER[ARMS] = {40, 25, 45, 60}; // cardume disputado, vila pagando caro

static uint32_t rng;
static int noise(int range) { // de -range a +range
    rng ^= rng << 13, rng ^= rng >> 17, rng ^= rng << 5;
    return (int)(rng % (uint32_t)(2 * range + 1)) - range;
}

typedef struct {
    int tried_all, best_before, best_after; // 1 = certo
    int chose_before, chose_after;          // viagens no melhor, nas 50 últimas de cada fase
    int switched;                           // em quantas viagens depois da mudança passou a achar a vila melhor
} result_t;

static void trip(brain_t *b, const int *pay, int n, int verbose) {
    int e = brain_choose(b);
    int r = pay[e] + noise(15); // sorte: cada viagem rende ±15 em volta da média
    brain_reward(b, r);
    if (verbose)
        printf("viagem %3d  %-15s %s rendeu %3d | médias %3d %3d %3d %3d | ε %2d%%\n", n, NAMES[e],
               b->explored ? "(explorou)" : "          ", r, b->value[0], b->value[1], b->value[2], b->value[3],
               b->epsilon / 10);
}

static result_t play(uint32_t seed, int verbose) {
    result_t res = {0};
    brain_t b;
    brain_init(&b, ARMS, seed);
    rng = seed * 2654435761u | 1;
    for (int t = 0; t < TRIPS; t++) {
        trip(&b, PAY_BEFORE, t + 1, verbose);
        if (t >= TRIPS - 50 && b.current == 1) res.chose_before++;
    }
    res.tried_all = b.tries[0] && b.tries[1] && b.tries[2] && b.tries[3];
    res.best_before = brain_best(&b) == 1;
    if (verbose) printf("--- o mundo mudou: o cardume ficou disputado e a vila paga caro ---\n");
    res.switched = -1;
    for (int t = 0; t < TRIPS; t++) {
        trip(&b, PAY_AFTER, TRIPS + t + 1, verbose);
        if (res.switched < 0 && brain_best(&b) == 3) res.switched = t + 1;
        if (t >= TRIPS - 50 && b.current == 3) res.chose_after++;
    }
    res.best_after = brain_best(&b) == 3;
    return res;
}

static int failures;
static void check(int ok, const char *what) {
    printf("  %s %s\n", ok ? "ok    " : "FALHOU", what);
    if (!ok) failures++;
}

int main(int argc, char **argv) {
    int verbose = argc > 1 && strcmp(argv[1], "-v") == 0;
    int tried = 0, before = 0, after = 0, chose_before = 0, chose_after = 0, never = 0;
    int switch_times[SEEDS];
    for (int s = 0; s < SEEDS; s++) {
        result_t r = play((uint32_t)s + 1, verbose && s == 0);
        tried += r.tried_all, before += r.best_before, after += r.best_after;
        chose_before += r.chose_before, chose_after += r.chose_after;
        switch_times[s] = r.switched < 0 ? 9999 : r.switched;
        never += r.switched < 0;
    }
    // Mediana de quanto demorou para trocar (ordena com inserção, são só 100).
    for (int i = 1; i < SEEDS; i++)
        for (int j = i; j > 0 && switch_times[j] < switch_times[j - 1]; j--) {
            int t = switch_times[j];
            switch_times[j] = switch_times[j - 1], switch_times[j - 1] = t;
        }

    printf("%d sementes, %d viagens antes e %d depois da mudança:\n", SEEDS, TRIPS, TRIPS);
    printf("  antes:  achou o cardume melhor em %d, e foi nele em %d%% das 50 últimas viagens\n", before,
           chose_before * 100 / (SEEDS * 50));
    printf("  depois: achou a vila melhor em %d, e foi nela em %d%% das 50 últimas viagens\n", after,
           chose_after * 100 / (SEEDS * 50));
    printf("  trocou para a vila depois de %d viagens (mediana)\n", switch_times[SEEDS / 2]);
    check(tried == SEEDS, "testou todas as estratégias");
    check(before == SEEDS, "sempre acha o cardume melhor antes da mudança");
    check(chose_before * 100 / (SEEDS * 50) >= 80, "vai no cardume em pelo menos 80% das viagens");
    check(after >= SEEDS * 98 / 100 && never <= 2, "percebe a mudança (98 de 100 sementes)");
    check(chose_after * 100 / (SEEDS * 50) >= 80, "no fim vai na vila em pelo menos 80% das viagens");

    // Mesma semente, mesmas escolhas: é o que deixa a simulação repetir igual.
    brain_t x, y;
    brain_init(&x, ARMS, 99), brain_init(&y, ARMS, 99);
    int same = 1;
    for (int t = 0; t < 500; t++) {
        int ex = brain_choose(&x), ey = brain_choose(&y);
        if (ex != ey) same = 0;
        brain_reward(&x, PAY_BEFORE[ex] + t % 7), brain_reward(&y, PAY_BEFORE[ey] + t % 7);
    }
    check(same, "mesma semente faz as mesmas escolhas");

    printf("%s\n", failures ? "NÃO PASSOU" : "PASSOU");
    return failures ? 1 : 0;
}
