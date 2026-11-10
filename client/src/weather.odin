package game

// Chuva e neblina na tela (v0.9). O servidor diz qual é o clima (s.weather);
// aqui um shader desenha por cima do mapa:
//   - chuva: riscos inclinados caindo rápido e o céu um pouco mais escuro;
//   - neblina: limpa perto do barco e fechando até FOG_VIEW células, igual ao
//     que o servidor deixa ver, com fiapos que andam devagar.
// A troca de clima não é de um quadro para o outro: a chuva e a neblina
// entram e saem aos poucos (WEATHER_FADE segundos).

import rl "vendor:raylib"

FOG_VIEW :: 16 // igual ao game.c: na neblina, só se vê até 16 células
WEATHER_FADE :: 3 // segundos para a chuva ou a neblina chegar (ou ir embora)

WEATHER_FS :: SHADER_HEADER + `
uniform vec2 size;    // tamanho da tela, em px
uniform vec2 center;  // o nosso barco, em px na tela
uniform vec2 cell;    // tamanho de uma célula, em px
uniform float time;   // segundos
uniform float rain;   // 0 a 1
uniform float fog;    // 0 a 1

float hash(float n) { return fract(sin(n) * 43758.5453); }

// Ruído suave em 2D, para os fiapos de neblina.
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float n = i.x + i.y * 57.0;
    return mix(mix(hash(n), hash(n + 1.0), f.x), mix(hash(n + 57.0), hash(n + 58.0), f.x), f.y);
}

// Desenha top (com alfa) por cima de base.
vec4 over(vec4 base, vec4 top) {
    float a = top.a + base.a * (1.0 - top.a);
    if (a <= 0.0) return vec4(0.0);
    return vec4((top.rgb * top.a + base.rgb * base.a * (1.0 - top.a)) / a, a);
}

void main() {
    vec2 p = vec2(gl_FragCoord.x, size.y - gl_FragCoord.y);
    vec4 c = vec4(0.0);

    // Chuva: o céu fecha um pouco e riscos caem inclinados pelo vento.
    if (rain > 0.0) {
        c = over(c, vec4(0.03, 0.05, 0.10, 0.18 * rain));
        vec2 q = vec2(p.x + p.y * 0.25, p.y - time * 700.0); // inclina e faz cair
        float col = floor(q.x / 5.0);
        float drop = fract(q.y / 90.0 + hash(col) * 7.0);      // onde a gota está na coluna
        float streak = step(0.75, hash(col * 1.7)) * (1.0 - smoothstep(0.0, 0.18, drop));
        c = over(c, vec4(0.70, 0.80, 0.95, 0.45 * streak * rain));
    }

    // Neblina: limpa perto do barco e fecha até FOG_VIEW células.
    if (fog > 0.0) {
        float dist = length((p - center) / cell);  // em células, igual ao servidor
        float far = smoothstep(7.0, 16.0, dist);         // 16 = FOG_VIEW
        float wisp = noise(p / 90.0 + vec2(time * 0.15, time * 0.05)) * 0.5 +
                     noise(p / 37.0 - vec2(time * 0.08, 0.0)) * 0.25;
        float a = clamp(0.10 + 0.85 * far + 0.25 * (wisp - 0.4), 0.0, 0.95);
        c = over(c, vec4(0.62, 0.68, 0.74, a * fog));
    }
    OUT_COLOR = c;
}
`

@(private = "file")
Weather_FX :: struct {
	shader:       rl.Shader,
	size, center: rl.ShaderLocationIndex,
	cell, time:   rl.ShaderLocationIndex,
	rain_loc:     rl.ShaderLocationIndex,
	fog_loc:      rl.ShaderLocationIndex,
	rain, fog:    f32, // quanto de chuva e de neblina está na tela agora
}

@(private = "file")
fx: Weather_FX

weather_init :: proc() {
	fx.shader = rl.LoadShaderFromMemory(nil, WEATHER_FS)
	fx.size = rl.ShaderLocationIndex(rl.GetShaderLocation(fx.shader, "size"))
	fx.center = rl.ShaderLocationIndex(rl.GetShaderLocation(fx.shader, "center"))
	fx.cell = rl.ShaderLocationIndex(rl.GetShaderLocation(fx.shader, "cell"))
	fx.time = rl.ShaderLocationIndex(rl.GetShaderLocation(fx.shader, "time"))
	fx.rain_loc = rl.ShaderLocationIndex(rl.GetShaderLocation(fx.shader, "rain"))
	fx.fog_loc = rl.ShaderLocationIndex(rl.GetShaderLocation(fx.shader, "fog"))
}

// Anda `v` em direção a `target`, no máximo `step`.
@(private = "file")
approach :: proc(v, target, step: f32) -> f32 {
	if v < target do return min(v + step, target)
	return max(v - step, target)
}

// Chuva e neblina por cima do mapa. (ox, oy) = célula no canto da tela.
draw_weather :: proc(ox, oy: i32) {
	step := rl.GetFrameTime() / WEATHER_FADE
	want_rain: f32 = 1 if s.weather == WEATHER_RAIN else 0
	want_fog: f32 = 1 if s.weather == WEATHER_FOG else 0
	fx.rain = approach(fx.rain, want_rain, step)
	fx.fog = approach(fx.fog, want_fog, step)
	if fx.rain <= 0 && fx.fog <= 0 do return

	size := [2]f32{f32(rl.GetScreenWidth()), f32(rl.GetScreenHeight())}
	center := [2]f32{f32((s.x - ox) * CELL_W + CELL_W / 2), f32((s.y - oy) * CELL_H + CELL_H / 2)}
	cell := [2]f32{CELL_W, CELL_H}
	now := rl.GetTime()
	time := f32(now - 1000 * f64(int(now / 1000)))
	rl.SetShaderValue(fx.shader, fx.size, &size, .VEC2)
	rl.SetShaderValue(fx.shader, fx.center, &center, .VEC2)
	rl.SetShaderValue(fx.shader, fx.cell, &cell, .VEC2)
	rl.SetShaderValue(fx.shader, fx.time, &time, .FLOAT)
	rl.SetShaderValue(fx.shader, fx.rain_loc, &fx.rain, .FLOAT)
	rl.SetShaderValue(fx.shader, fx.fog_loc, &fx.fog, .FLOAT)
	rl.BeginShaderMode(fx.shader)
	rl.DrawRectangle(0, 0, rl.GetScreenWidth() - PANEL_W, rl.GetScreenHeight(), rl.WHITE)
	rl.EndShaderMode()
}

// Nome do clima para o painel.
weather_name :: proc(kind: u8) -> string {
	switch kind {
	case WEATHER_RAIN: return "Chuva"
	case WEATHER_FOG: return "Neblina"
	}
	return "Tempo limpo"
}
