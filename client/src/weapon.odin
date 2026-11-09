package game

// Mesma tabela de server/components/game/weapon.c, só para a mira mostrar
// o alcance e o painel mostrar a recarga. Quem decide o tiro é o servidor.

Weapon :: struct {
	range:  int, // células
	reload: int, // ticks de 100 ms
	speed:  int, // células por tick
}

// Indexada pelo tipo do item: 0 é ITEM_NONE (não é arma), 1 arpão,
// 2 mosquete, 3 canhão leve. Os outros itens não são armas.
WEAPONS := [4]Weapon {
	{},
	{range = 8, reload = 6, speed = 2},
	{range = 14, reload = 12, speed = 3},
	{range = 18, reload = 20, speed = 2},
}

// A arma equipada (slot 1 do barco), ou ok = false se não tem nenhuma.
equipped_weapon :: proc() -> (w: Weapon, base: u8, ok: bool) {
	for it in s.bag {
		if it.equipped == 1 && int(it.base) < len(WEAPONS) && WEAPONS[it.base].range > 0 {
			return WEAPONS[it.base], it.base, true
		}
	}
	return {}, 0, false
}