package game

// Dia, noite e luz (v0.9).
// O relógio do dia agora vem do servidor (o tick do S_STATE): todo mundo vê
// o pôr do sol na mesma hora. À noite, um shader escurece a tela inteira e
// abre buracos de luz: a lanterna do nosso barco, a dos outros barcos e os
// faróis do porto e das vilas, com o facho girando.

import "core:math"
import rl "vendor:raylib"

TICK_HZ :: 10 // igual ao game.h
DAY_SECONDS :: 600 // um dia inteiro (dia + noite) dura 10 minutos
MAX_LIGHTS :: 16 // tem que bater com o tamanho do array no shader

LANTERN_R :: 170 // raio da lanterna do nosso barco, em px
BOAT_LIGHT_R :: 90 // lanterna dos outros barcos
LIGHTHOUSE_R :: 70 // brilho em volta do farol
BEAM_SPEED :: 0.8 // voltas do facho, em radianos por segundo
NO_BEAM :: -1 // w de uma luz sem facho

LIGHT_FS :: SHADER_HEADER + `
uniform vec2 size;          // tamanho da tela, em px
uniform float night;        // 0 de dia, 1 no meio da noite
uniform vec4 lights[16];    // x, y (px na tela), raio, ângulo do facho (< 0: sem facho)
uniform int count;

void main() {
    // gl_FragCoord começa embaixo; a tela começa em cima.
    vec2 p = vec2(gl_FragCoord.x, size.y - gl_FragCoord.y);
    float dark = 1.0;  // quanto do escuro sobra (1 = todo)
    float warm = 0.0;  // brilho amarelado perto das luzes
    for (int i = 0; i < 16; i++) {
        if (i >= count) break;
        vec4 l = lights[i];
        vec2 d = p - l.xy;
        float dist = length(d);
        float lit = 1.0 - smoothstep(0.3 * l.z, l.z, dist);
        if (l.w >= 0.0) {
            // Facho do farol: uma fatia estreita de 420 px que some aos poucos.
            float ang = atan(d.y, d.x) - l.w;
            ang = abs(mod(ang + 3.14159, 6.28318) - 3.14159);
            float beam = (1.0 - smoothstep(0.04, 0.12, ang)) * (1.0 - smoothstep(210.0, 420.0, dist));
            lit = max(lit, beam * 0.85);
        }
        dark *= 1.0 - lit;
        warm += lit * 0.5;
    }
    float a = night * 0.67 * dark;
    float w = night * 0.12 * min(warm, 1.0);
    vec3 color = mix(vec3(0.01, 0.025, 0.09), vec3(1.0, 0.8, 0.5), w / (a + w + 0.001));
    OUT_COLOR = vec4(color, a + w);
}
`

@(private = "file")
Light :: struct {
	shader: rl.Shader,
	size:   rl.ShaderLocationIndex,
	night:  rl.ShaderLocationIndex,
	lights: rl.ShaderLocationIndex,
	count:  rl.ShaderLocationIndex,
}

@(private = "file")
light: Light

light_init :: proc() {
	light.shader = rl.LoadShaderFromMemory(nil, LIGHT_FS)
	light.size = rl.ShaderLocationIndex(rl.GetShaderLocation(light.shader, "size"))
	light.night = rl.ShaderLocationIndex(rl.GetShaderLocation(light.shader, "night"))
	light.lights = rl.ShaderLocationIndex(rl.GetShaderLocation(light.shader, "lights"))
	light.count = rl.ShaderLocationIndex(rl.GetShaderLocation(light.shader, "count"))
}

// Segundos do relógio do mundo: o tick do servidor, ou o relógio local no
// modo offline (e antes do primeiro S_STATE).
world_time :: proc() -> f64 {
	if s.offline || s.tick == 0 do return rl.GetTime()
	return f64(s.tick) / TICK_HZ
}

// 0 de dia, 1 no meio da noite. O sol nasce e se põe aos poucos.
night_amount :: proc() -> f32 {
	day := math.sin(world_time() * math.TAU / DAY_SECONDS)
	return f32(clamp(-day * 1.4, 0, 1))
}

// Escurece a tela, menos onde há luz. (ox, oy) = célula no canto da tela.
draw_night :: proc(ox, oy: i32) {
	night := night_amount()
	if night <= 0 do return

	ls: Lights
	// A lanterna treme um pouco, como uma chama.
	flicker := f32(1 + 0.04 * math.sin(rl.GetTime() * 13) + 0.03 * math.sin(rl.GetTime() * 7.3))
	add_light(&ls, ox, oy, s.x, s.y, LANTERN_R * flicker, NO_BEAM)
	for b in s.boats[:s.boat_count] do add_light(&ls, ox, oy, b.x, b.y, BOAT_LIGHT_R, NO_BEAM)
	// Faróis: no cais do porto e no de cada vila, cada um com o facho num ângulo.
	spin := f32(math.mod(rl.GetTime() * BEAM_SPEED, math.TAU))
	add_light(&ls, ox, oy, DOCK_X, DOCK_Y, LIGHTHOUSE_R, spin)
	k: f32 = 1
	for m in s.marks[:s.mark_count] {
		if m.kind != MARK_VILLAGE do continue
		add_light(&ls, ox, oy, m.x, m.y, LIGHTHOUSE_R, f32(math.mod(f64(spin + k * 2.1), math.TAU)))
		k += 1
	}

	size := [2]f32{f32(rl.GetScreenWidth()), f32(rl.GetScreenHeight())}
	rl.SetShaderValue(light.shader, light.size, &size, .VEC2)
	rl.SetShaderValue(light.shader, light.night, &night, .FLOAT)
	rl.SetShaderValueV(light.shader, light.lights, &ls.data, .VEC4, ls.count)
	rl.SetShaderValue(light.shader, light.count, &ls.count, .INT)
	rl.BeginShaderMode(light.shader)
	rl.DrawRectangle(0, 0, rl.GetScreenWidth() - PANEL_W, rl.GetScreenHeight(), rl.WHITE)
	rl.EndShaderMode()
}

@(private = "file")
Lights :: struct {
	data:  [MAX_LIGHTS][4]f32, // x, y, raio, ângulo do facho
	count: i32,
}

// Uma luz na célula (x, y) do mundo, convertida para px na tela.
@(private = "file")
add_light :: proc(ls: ^Lights, ox, oy, x, y: i32, r, beam: f32) {
	if ls.count >= MAX_LIGHTS do return
	px := f32((x - ox) * CELL_W + CELL_W / 2)
	py := f32((y - oy) * CELL_H + CELL_H / 2)
	ls.data[ls.count] = {px, py, r, beam}
	ls.count += 1
}
