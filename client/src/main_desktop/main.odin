package main_desktop

// Versão desktop (Windows/Linux/macOS), sem rede: abre em modo offline.
// Útil para mexer no visual sem compilar para a web: odin run src/main_desktop

import game ".."

main :: proc() {
	game.init()
	for game.should_run() do game.update()
	game.shutdown()
}
