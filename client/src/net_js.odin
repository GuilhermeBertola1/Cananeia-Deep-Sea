#+build js
package game

// Rede no navegador: WebSocket do emscripten (linkado com -lwebsocket.js).
// Os callbacks chegam entre um quadro e outro, então dá para tratar a
// mensagem direto, sem fila.

import "base:runtime"
import "core:c"
import "core:strings"

@(private = "file")
Ws_Attrs :: struct {
	url:                   cstring,
	protocols:             cstring,
	create_on_main_thread: bool,
}

@(private = "file")
Ws_Message_Event :: struct {
	socket:    c.int,
	data:      [^]u8,
	num_bytes: u32,
	is_text:   bool,
}

@(private = "file")
Ws_Callback :: #type proc "c" (event_type: c.int, event: rawptr, user: rawptr) -> bool

@(private = "file")
CALLING_THREAD :: uintptr(2) // EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD

@(private = "file", default_calling_convention = "c")
foreign {
	emscripten_websocket_new :: proc(attrs: ^Ws_Attrs) -> c.int ---
	emscripten_websocket_send_binary :: proc(socket: c.int, data: rawptr, len: u32) -> c.int ---
	emscripten_websocket_delete :: proc(socket: c.int) -> c.int ---
	emscripten_websocket_set_onopen_callback_on_thread :: proc(socket: c.int, user: rawptr, cb: Ws_Callback, thread: uintptr) -> c.int ---
	emscripten_websocket_set_onmessage_callback_on_thread :: proc(socket: c.int, user: rawptr, cb: Ws_Callback, thread: uintptr) -> c.int ---
	emscripten_websocket_set_onclose_callback_on_thread :: proc(socket: c.int, user: rawptr, cb: Ws_Callback, thread: uintptr) -> c.int ---
	emscripten_run_script_string :: proc(script: cstring) -> cstring ---
}

@(private = "file")
socket: c.int = -1
@(private = "file")
net_ctx: runtime.Context

// O servidor (ESP32 ou PC) é quem serve a página, então o jogo fica no mesmo endereço.
net_default_url :: proc() -> string {
	url := emscripten_run_script_string("(location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host + '/ws'")
	return strings.clone(string(url)) // o emscripten reaproveita esse buffer
}

net_connect :: proc(url: string) -> bool {
	net_ctx = context
	if socket >= 0 do emscripten_websocket_delete(socket)
	attrs := Ws_Attrs{url = strings.clone_to_cstring(url, context.temp_allocator), create_on_main_thread = true}
	socket = emscripten_websocket_new(&attrs)
	if socket <= 0 {socket = -1; return false}
	emscripten_websocket_set_onopen_callback_on_thread(socket, nil, on_open, CALLING_THREAD)
	emscripten_websocket_set_onmessage_callback_on_thread(socket, nil, on_message, CALLING_THREAD)
	emscripten_websocket_set_onclose_callback_on_thread(socket, nil, on_close, CALLING_THREAD)
	return true
}

net_send :: proc(data: []u8) {
	if socket >= 0 do emscripten_websocket_send_binary(socket, raw_data(data), u32(len(data)))
}

@(private = "file")
on_open :: proc "c" (_: c.int, _: rawptr, _: rawptr) -> bool {
	context = net_ctx
	on_net_open()
	return true
}

@(private = "file")
on_message :: proc "c" (_: c.int, event: rawptr, _: rawptr) -> bool {
	context = net_ctx
	e := (^Ws_Message_Event)(event)
	if !e.is_text do on_net_message(e.data[:e.num_bytes])
	return true
}

@(private = "file")
on_close :: proc "c" (_: c.int, _: rawptr, _: rawptr) -> bool {
	context = net_ctx
	emscripten_websocket_delete(socket)
	socket = -1
	on_net_close()
	return true
}
