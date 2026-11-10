package game

// Mesma tabela de server/components/game/weapon.c, só para a mira mostrar
// o alcance e o painel mostrar a recarga. Quem decide o tiro é o servidor.

Weapon :: struct {
	range:  int, // células
	reload: int, // ticks de 100 ms
	speed:  int, // células por tick
	fx:     u8,  // poder das armas únicas (FX_*)
}

// Mesmos valores de weapon_fx_t em weapon.h.
FX_NONE :: 0
FX_LEECH :: 1
FX_GRAB :: 2
FX_SPREAD :: 3

// Indexada pelo tipo do item: 1 arpão, 2 mosquete, 3 canhão leve e, de 11 a 13,
// as armas únicas dos bosses. Os índices que faltam não são armas (range 0).
WEAPONS := [14]Weapon {
	1  = {range = 8, reload = 6, speed = 2},
	2  = {range = 14, reload = 12, speed = 3},
	3  = {range = 18, reload = 20, speed = 2},
	11 = {range = 10, reload = 8, speed = 3, fx = FX_LEECH},
	12 = {range = 12, reload = 10, speed = 3, fx = FX_GRAB},
	13 = {range = 16, reload = 15, speed = 2, fx = FX_SPREAD},
}

// Os dados de arma de um item (ok = false se não é arma).
weapon_of :: proc(base: u8) -> (w: Weapon, ok: bool) {
	if int(base) >= len(WEAPONS) || WEAPONS[base].range == 0 do return {}, false
	return WEAPONS[base], true
}

// O que o poder especial faz, para o inventário.
fx_text :: proc(fx: u8) -> string {
	switch fx {
	case FX_LEECH: return "Conserta seu barco com 25% do dano"
	case FX_GRAB: return "Prende o alvo parado por 2 s"
	case FX_SPREAD: return "Atira 3 tiros em leque"
	}
	return ""
}

// A arma equipada (slot 1 do barco), ou ok = false se não tem nenhuma.
equipped_weapon :: proc() -> (w: Weapon, base: u8, ok: bool) {
	for it in s.bag {
		if wp, ok := weapon_of(it.base); ok && it.equipped == 1 do return wp, it.base, true
	}
	return {}, 0, false
}