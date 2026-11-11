package game

import rl "vendor:raylib"

// Mesmas tabelas de server/components/game/items.c. A ordem é o id na rede.
// Só os nomes: os números de cada item já chegam prontos do servidor.

BAG_SIZE :: 12
STAT_COUNT :: 5
MAX_AFFIXES :: 3
AFFIX_NONE :: 0xFF
RARITY_COMMON :: 0
RARITY_RARE :: 1
RARITY_EPIC :: 2
RARITY_LEGENDARY :: 3
ENCHANT_MAX :: 3
ITEM_NONE :: 0
ITEM_DENTE :: 11 // a primeira arma única (as de boss vêm depois de todos os outros itens)

Item :: struct {
	base:     u8, // ITEM_NONE = slot vazio
	rarity:   u8,
	equipped: u8, // 0 = não; 1 arma, 2 ferramenta, 3 a 5 boosts
	enchant:  u8, // nível do encanto da sereia (0 a 3), já somado nos stats
	affix:    [MAX_AFFIXES]u8,
	stats:    [STAT_COUNT]int,
}

ITEM_NAMES := [?]string {
	"",
	"Arpão",
	"Mosquete",
	"Canhão leve",
	"Vara de bambu",
	"Rede de pesca",
	"Rede de arrasto",
	"Motor a vapor",
	"Casco reforçado",
	"Amuleto de búzios",
	"Isca encantada",
	"Dente do Megalodon",
	"Tentáculo do Kraken",
	"Cajado de R'lyeh",
}

AFFIX_NAMES := [?]string{"Veloz", "Robusto", "Afiado", "do Pescador", "da Sorte"}

STAT_NAMES := [STAT_COUNT]string{"Velocidade", "Vida", "Dano", "Produção", "Sorte"}
STAT_UNITS := [STAT_COUNT]string{"", "", "", "%", ""}

RARITY_NAMES := [?]string{"comum", "raro", "épico", "lendário"}
RARITY_COLORS := [?]rl.Color {
	{215, 227, 244, 255}, // comum: branco
	{90, 170, 255, 255}, // raro: azul
	{199, 125, 255, 255}, // épico: roxo
	{255, 170, 60, 255}, // lendário: laranja
}

EQUIP_NAMES := [?]string{"", "arma", "ferramenta", "boost", "boost", "boost"}

// Mecânico: preço pela raridade mínima do item que sai (igual ao game.c).
COMBINE_PRICE := [4]u32{0, 8000, 25000, 60000}

// Raridade mínima do item que o mecânico faz juntando a e b: a do melhor
// dos dois, ou uma acima se forem iguais. Mesma conta do servidor.
combine_rarity :: proc(a, b: Item) -> u8 {
	r := max(a.rarity, b.rarity)
	if a.rarity == b.rarity && r < RARITY_LEGENDARY do r += 1
	return r
}

// Arma única de boss: sempre lendária, nunca sai de baú.
item_is_unique :: proc(base: u8) -> bool {
	return base >= ITEM_DENTE && int(base) < len(ITEM_NAMES)
}

item_name :: proc(base: u8) -> string {
	return ITEM_NAMES[base] if int(base) < len(ITEM_NAMES) else "?"
}

affix_name :: proc(id: u8) -> string {
	return AFFIX_NAMES[id] if int(id) < len(AFFIX_NAMES) else "?"
}

rarity_name :: proc(r: u8) -> string {
	return RARITY_NAMES[r] if int(r) < len(RARITY_NAMES) else "?"
}

// Cor do item na lista: a da raridade, ou vermelho-dourado na arma única.
COLOR_UNIQUE :: rl.Color{255, 110, 80, 255}

item_color :: proc(it: Item) -> rl.Color {
	return COLOR_UNIQUE if item_is_unique(it.base) else rarity_color(it.rarity)
}

rarity_color :: proc(r: u8) -> rl.Color {
	return RARITY_COLORS[r] if int(r) < len(RARITY_COLORS) else COLOR_INK
}