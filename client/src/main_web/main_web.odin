package main_web

// Entrada da versão web. O index.html chama estas funções:
// main_start uma vez, main_update a cada quadro e
// web_window_size_changed quando a janela muda de tamanho.

import "base:runtime"
import "core:c"
import "core:mem"
import rl "vendor:raylib"
import game ".."

@(private = "file")
web_context: runtime.Context

@(export)
main_start :: proc "c" () {
	context = runtime.default_context()
	context.allocator = emscripten_allocator()
	runtime.init_global_temporary_allocator(4 * mem.Megabyte)
	web_context = context
	game.init()
}

@(export)
main_update :: proc "c" () -> bool {
	context = web_context
	game.update()
	return true
}

@(export)
web_window_size_changed :: proc "c" (w, h: c.int) {
	context = web_context
	rl.SetWindowSize(w, h)
}
