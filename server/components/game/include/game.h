// Núcleo do jogo, igual no ESP32 e no servidor de teste do PC.
// O servidor é a autoridade: o cliente só manda intenções (direção, vender)
// e o jogo decide posição, pesca, dinheiro e ranking.
//
// Nada aqui é thread-safe: quem chama (transporte) garante uma chamada por vez.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "fish.h"

// Os 8 primeiros slots são de gente (o transporte só usa esses); o resto é
// dos pescadores NPC, que são barcos de verdade, mas sem conexão.
#define HUMAN_SLOTS 8
#define MAX_PLAYERS 24
#define NPC_MAX (MAX_PLAYERS - HUMAN_SLOTS)
#define TICK_HZ 10
#define VIEW_RADIUS 50

// Como o jogo manda bytes para um jogador. O transporte implementa.
typedef void (*game_send_fn)(int slot, const uint8_t *data, size_t len);

// O mapa é gerado aos poucos, em chunks (chunks.h), conforme os barcos andam.
void game_init(uint32_t seed, uint32_t random_seed, game_send_fn send);

int game_connect(void);         // reserva um slot para uma conexão nova; -1 se lotado
void game_disconnect(int slot); // salva o perfil e libera o slot
void game_receive(int slot, const uint8_t *data, size_t len);
void game_tick(void);           // chamar TICK_HZ vezes por segundo
void game_save_all(void);
int game_online(void);          // quantas pessoas estão jogando agora (para o /status)
int game_add_npcs(int n);       // põe até n pescadores NPC no mar; devolve quantos entraram
size_t game_chunks(void);       // quantos chunks do mapa estão na memória (para o /status)

// Estratégias dos pescadores NPC: os "braços" do cérebro (brain.h).
enum { NPC_NEAR_PORT = 0, NPC_SCHOOL, NPC_FULL_STOCK, NPC_VILLAGE, NPC_STRATEGIES };

// Retrato do mundo para a simulação sem rede (server/host/sim.c) e, mais
// tarde, para os agentes. Só lê: não muda nada no jogo.
typedef struct {
    uint32_t tick;
    int online;            // pessoas jogando (sem os NPCs)
    uint64_t money;        // dinheiro somado delas, em centavos
    uint32_t hold;         // peixes nos porões delas
    uint32_t sold;         // peixes vendidos desde o game_init
    uint64_t earned;       // quanto essas vendas pagaram, em centavos
    int schools;           // cardumes vivos
    int predators;         // tubarões (e navio fantasma) vivos
    int villages;          // vilas já descobertas
    int stock_regions;     // regiões pescadas que ainda não encheram
    size_t chunks;         // chunks do mapa na memória
    uint32_t price_port[FISH_COUNT]; // preço de cada espécie no porto, em centavos
    uint32_t price_avg[FISH_COUNT];  // média de todos os balcões, em centavos
    // Pescadores NPC
    int npcs;
    uint64_t npc_money;              // dinheiro somado, em centavos
    uint32_t npc_trips, npc_deaths;  // viagens terminadas (venderam) e mortes
    uint32_t npc_tows;               // vezes que encalharam e foram rebocados
    int npc_using[NPC_STRATEGIES];   // quantos estão usando cada estratégia agora
    int npc_best[NPC_STRATEGIES];    // quantos acham cada uma a melhor (maior média)
} game_stats_t;

void game_stats(game_stats_t *out);
