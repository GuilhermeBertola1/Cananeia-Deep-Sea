#pragma once
#include <stdint.h>

// Poder especial das armas únicas (as comuns não têm nenhum).
typedef enum {
    FX_NONE = 0,
    FX_LEECH,  // Dente: 25% do dano volta como conserto do próprio barco
    FX_GRAB,   // Tentáculo: o alvo fica preso, parado, por 2 s
    FX_SPREAD, // Cajado: três tiros em leque
} weapon_fx_t;

typedef struct {
    uint8_t range;  // células até o tiro sumir
    uint8_t reload; // ticks entre um tiro e outro (10 ticks = 1 s)
    uint8_t speed;  // células por tick
    uint8_t fx;     // weapon_fx_t
} weapon_t;

// NULL se o item não é arma.
const weapon_t *weapon_of(uint8_t item_base);