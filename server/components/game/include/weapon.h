#pragma once
#include <stdint.h>

typedef struct {
    uint8_t range;  // células até o tiro sumir
    uint8_t reload; // ticks entre um tiro e outro (10 ticks = 1 s)
    uint8_t speed;  // células por tick
} weapon_t;

// NULL se o item não é arma.
const weapon_t *weapon_of(uint8_t item_base);