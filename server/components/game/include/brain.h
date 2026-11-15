// Cérebro dos agentes: um "bandido multibraço" ε-guloso.
//
// Cada braço é uma estratégia (por exemplo "pescar perto do porto" ou "caçar
// cardume"). O agente escolhe uma, joga com ela, e no fim conta quanto rendeu
// (a recompensa). O cérebro guarda uma média de cada braço que esquece o
// passado devagar, então se o mundo muda a média muda junto.
//
// Para escolher: quase sempre o braço de maior média (aproveitar), e de vez em
// quando, com chance ε, o braço que está há mais tempo sem ser jogado
// (explorar: conferir se o que ficou esquecido melhorou). O ε começa alto e
// cai até um mínimo, mas nunca zera.
//
// Tudo em inteiros e com o sorteio próprio de cada cérebro: mesma semente,
// mesmas escolhas, em qualquer máquina.
#pragma once
#include <stdint.h>

#define BRAIN_MAX_ARMS 8
#define BRAIN_RATE_PCT 30          // a média anda 30% do caminho até cada resultado novo
#define BRAIN_EPSILON_START 300    // 30% de exploração no começo (em milésimos)
#define BRAIN_EPSILON_MIN 100      // e nunca menos que 10%
#define BRAIN_EPSILON_DECAY_PCT 95 // a cada escolha, ε vira 95% do que era

typedef struct {
    uint8_t arms;                   // quantos braços (estratégias)
    uint8_t current;                // o braço da rodada de agora
    uint8_t explored;               // 1 se a última escolha foi exploração
    uint16_t epsilon;               // chance de explorar agora, em milésimos
    uint16_t tries[BRAIN_MAX_ARMS]; // quantas vezes cada braço foi jogado
    uint32_t round;                 // quantas escolhas já fez
    uint32_t last[BRAIN_MAX_ARMS];  // em que escolha cada braço foi jogado por último
    int32_t value[BRAIN_MAX_ARMS];  // média de cada braço, na unidade da recompensa
    uint32_t rng;
} brain_t;

void brain_init(brain_t *b, int arms, uint32_t seed);
// Escolhe o braço da próxima rodada (e guarda em b->current).
int brain_choose(brain_t *b);
// Conta quanto a rodada rendeu, para o braço b->current.
void brain_reward(brain_t *b, int32_t reward);
// O braço de maior média (o que ele acha melhor agora), sem sortear nada.
int brain_best(const brain_t *b);
