package game

import rl "vendor:raylib"

// Mesma tabela de server/components/game/fish.c. A ordem é o id na rede.
Species :: struct {
	name:        string,
	price_cents: int, // preço sem pressão de mercado
	biome:       Tile,
	color:       rl.Color,
}

FISH_COUNT :: 9
FISH_NONE :: 0xFF

SPECIES := [FISH_COUNT]Species {
	{"Sardinha", 100, .Shallow, {200, 220, 230, 255}},
	{"Camarão", 200, .Shallow, {255, 140, 120, 255}},
	{"Caranguejo", 400, .Shallow, {240, 90, 70, 255}},
	{"Robalo", 400, .Water, {170, 200, 160, 255}},
	{"Atum", 600, .Water, {120, 160, 230, 255}},
	{"Tartaruga", 1200, .Water, {110, 200, 120, 255}},
	{"Polvo", 1000, .Deep, {220, 120, 220, 255}},
	{"Peixe-espada", 1500, .Deep, {200, 210, 255, 255}},
	{"Lula-gigante", 4000, .Deep, {255, 110, 160, 255}},
}

species_name :: proc(id: int) -> string {
	return SPECIES[id].name if id >= 0 && id < FISH_COUNT else "?"
}
