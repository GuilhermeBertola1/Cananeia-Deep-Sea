#+build !js
package game

// No desktop ainda não há rede: o jogo abre em modo offline (só navegar),
// útil para testar o visual sem compilar para a web.

net_default_url :: proc() -> string {return ""}

net_connect :: proc(url: string) -> bool {return false}

net_send :: proc(data: []u8) {}
