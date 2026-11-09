// Upgrades do barco, vendidos pelo Estaleiro no porto. Cada peça tem 5
// níveis (0 a 4); o perfil guarda só o número do nível.
//
// A mesma tabela existe em client/src/boat.odin, para a tela do estaleiro
// mostrar preços e efeitos; se mudar aqui, mude lá também.
#pragma once
#include <stdint.h>

#define BOAT_LEVELS 5

typedef enum { PART_SAIL = 0, PART_HULL, PART_RUDDER, PART_COUNT } boat_part_t;

// Quanto custa subir PARA o nível i (centavos). O nível 0 já vem no barco.
extern const uint32_t UPGRADE_PRICE[BOAT_LEVELS];

extern const uint8_t SAIL_SPEED[BOAT_LEVELS];        // células/s a mais
extern const uint16_t HULL_CAPACITY[BOAT_LEVELS];    // peixes no porão
extern const uint16_t HULL_HP[BOAT_LEVELS];          // vida a mais
extern const uint8_t RUDDER_TURN_TICKS[BOAT_LEVELS]; // ticks para virar 45°

// Reparo no estaleiro: preço por ponto de vida que falta.
#define REPAIR_CENTS_PER_HP 20