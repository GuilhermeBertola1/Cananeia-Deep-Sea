#include "boat.h"

// Vela 4 + motor a vapor lendário passa do antigo limite de 12 células/s;
// por isso MAX_SPEED subiu para 16 no game.c.
const uint32_t UPGRADE_PRICE[BOAT_LEVELS] = {0, 15000, 40000, 100000, 250000}; // $150, $400, $1000, $2500
const uint8_t SAIL_SPEED[BOAT_LEVELS] = {0, 1, 2, 3, 5};
const uint16_t HULL_CAPACITY[BOAT_LEVELS] = {150, 200, 260, 330, 420};
const uint16_t HULL_HP[BOAT_LEVELS] = {0, 25, 55, 90, 140};
// 5 ticks = meio segundo para cada 45°: dar meia-volta leva 2 s no leme
// de fábrica e 0,4 s no melhor.
const uint8_t RUDDER_TURN_TICKS[BOAT_LEVELS] = {5, 4, 3, 2, 1};