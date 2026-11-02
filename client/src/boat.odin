package game

import "core:fmt"

// Mesma tabela de server/components/game/boat.c, só para a tela do
// estaleiro mostrar preço e efeito de cada nível. Quem decide é o servidor.

BOAT_LEVELS :: 5
PART_COUNT :: 3
PART_SAIL :: 0
PART_HULL :: 1
PART_RUDDER :: 2

PART_NAMES := [PART_COUNT]string{"Vela", "Casco", "Leme"}

UPGRADE_PRICE := [BOAT_LEVELS]u32{0, 15000, 40000, 100000, 250000} // centavos
SAIL_SPEED := [BOAT_LEVELS]int{0, 1, 2, 3, 5}
HULL_CAPACITY := [BOAT_LEVELS]int{150, 200, 260, 330, 420}
HULL_HP := [BOAT_LEVELS]int{0, 25, 55, 90, 140}
RUDDER_TURN_TICKS := [BOAT_LEVELS]int{5, 4, 3, 2, 1}

REPAIR_CENTS_PER_HP :: 20

part_name :: proc(part: int) -> string {
	return PART_NAMES[part] if part >= 0 && part < PART_COUNT else "?"
}

// O que a peça faz num nível, em texto curto para a tela.
part_effect :: proc(part, level: int) -> string {
	switch part {
	case PART_SAIL:
		return fmt.tprintf("+%d velocidade", SAIL_SPEED[level])
	case PART_HULL:
		return fmt.tprintf("porão %d, +%d vida", HULL_CAPACITY[level], HULL_HP[level])
	case PART_RUDDER:
		// 45° a cada N ticks de 100 ms.
		return fmt.tprintf("vira %d°/s", 450 / RUDDER_TURN_TICKS[level])
	}
	return ""
}