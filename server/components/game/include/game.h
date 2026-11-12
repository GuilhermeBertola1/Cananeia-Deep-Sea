// Núcleo do jogo, igual no ESP32 e no servidor de teste do PC.
// O servidor é a autoridade: o cliente só manda intenções (direção, vender)
// e o jogo decide posição, pesca, dinheiro e ranking.
//
// Nada aqui é thread-safe: quem chama (transporte) garante uma chamada por vez.
#pragma once
#include <stdint.h>
#include <stddef.h>

#define MAX_PLAYERS 8
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
int game_online(void);          // quantos estão jogando agora (para o /status)
size_t game_chunks(void);       // quantos chunks do mapa estão na memória (para o /status)
