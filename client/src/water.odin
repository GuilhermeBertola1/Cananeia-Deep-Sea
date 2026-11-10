package game

// Shader de água (v0.9). A camada do oceano é desenhada numa textura do
// tamanho da tela e depois vai para a tela passando por um shader que:
//   - empurra a imagem uns 2 px com ondas cruzadas (os ~ balançam);
//   - clareia a crista e escurece o vale de uma ondulação larga;
//   - acende um brilho curto onde as ondas se somam (o reflexo do sol).
// As ondas usam a posição no MUNDO, não na tela: quando o barco anda, o
// desenho das ondas fica parado no mar, como deve ser.

import rl "vendor:raylib"

// O mesmo shader roda na web (WebGL 1, GLSL 100) e no desktop (OpenGL 3.3,
// GLSL 330). Só o começo muda; as macros deixam o resto igual nos dois.
when ODIN_OS == .JS {
	SHADER_HEADER :: `#version 100
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#define IN varying
#define TEX texture2D
#define OUT_COLOR gl_FragColor
`
} else {
	SHADER_HEADER :: `#version 330
#define IN in
#define TEX texture
out vec4 outColor;
#define OUT_COLOR outColor
`
}

WATER_FS :: SHADER_HEADER + `
IN vec2 fragTexCoord;
IN vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 size;   // tamanho da textura, em px
uniform vec2 offset; // canto superior esquerdo da tela, em px do mundo
uniform float time;  // segundos
uniform float sun;   // 1 de dia, 0 à noite: sem sol, sem reflexo

void main() {
    // A textura de render vem de cabeça para baixo: y = 1 é o topo da tela.
    vec2 w = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y) * size + offset;

    // Duas ondas cruzadas deslocam a imagem (no máximo ~2 px).
    vec2 d = vec2(sin(w.y * 0.045 + time * 1.7) + 0.5 * sin((w.x + w.y) * 0.03 + time * 1.1),
                  cos(w.x * 0.04 + time * 1.3)) * 1.4;
    vec4 c = TEX(texture0, fragTexCoord + d / size);
    // Perto da costa a onda puxaria o buraco da ilha: aí fica sem deslocar.
    if (c.a < 0.5) c = TEX(texture0, fragTexCoord);

    // Ondulação larga: o mar fica um pouco mais claro na crista e mais escuro no vale.
    float wave = sin(w.x * 0.021 + time * 0.9) * sin(w.y * 0.033 - time * 0.7);
    c.rgb *= 0.92 + 0.14 * (wave * 0.5 + 0.5);

    // Reflexo do sol: só onde três ondas se somam lá no alto, um brilho curto.
    float h = wave + 0.6 * sin((w.x - w.y) * 0.05 + time * 2.1) +
              0.4 * sin(w.x * 0.11 + w.y * 0.07 - time * 3.0);
    float glint = smoothstep(1.5, 1.85, h);
    c.rgb += glint * sun * vec3(0.35, 0.45, 0.55);

    OUT_COLOR = c * colDiffuse * fragColor;
}
`

@(private = "file")
Water :: struct {
	shader: rl.Shader,
	target: rl.RenderTexture2D,
	size:   rl.ShaderLocationIndex,
	offset: rl.ShaderLocationIndex,
	time:   rl.ShaderLocationIndex,
	sun:    rl.ShaderLocationIndex,
}

@(private = "file")
water: Water

water_init :: proc() {
	// nil no vertex shader: usa o da raylib, que já manda fragTexCoord e fragColor.
	water.shader = rl.LoadShaderFromMemory(nil, WATER_FS)
	water.size = rl.ShaderLocationIndex(rl.GetShaderLocation(water.shader, "size"))
	water.offset = rl.ShaderLocationIndex(rl.GetShaderLocation(water.shader, "offset"))
	water.time = rl.ShaderLocationIndex(rl.GetShaderLocation(water.shader, "time"))
	water.sun = rl.ShaderLocationIndex(rl.GetShaderLocation(water.shader, "sun"))
}

// Tudo o que for desenhado entre water_begin e water_end vai para a textura.
water_begin :: proc() {
	w, h := rl.GetScreenWidth(), rl.GetScreenHeight()
	if water.target.texture.width != w || water.target.texture.height != h {
		// A janela mudou de tamanho (ou é o primeiro quadro): textura nova.
		if water.target.id != 0 do rl.UnloadRenderTexture(water.target)
		water.target = rl.LoadRenderTexture(w, h)
	}
	rl.BeginTextureMode(water.target)
	rl.ClearBackground(rl.BLANK)
}

// Fecha a textura e desenha ela na tela passando pelo shader.
// (ox, oy) = célula do mundo no canto superior esquerdo da tela.
water_end :: proc(ox, oy: i32) {
	rl.EndTextureMode()
	tex := water.target.texture
	size := [2]f32{f32(tex.width), f32(tex.height)}
	offset := [2]f32{f32(ox * CELL_W), f32(oy * CELL_H)}
	// O tempo dá a volta a cada 1000 s para não perder precisão no celular.
	now := rl.GetTime()
	time := f32(now - 1000 * f64(int(now / 1000)))
	rl.SetShaderValue(water.shader, water.size, &size, .VEC2)
	rl.SetShaderValue(water.shader, water.offset, &offset, .VEC2)
	rl.SetShaderValue(water.shader, water.time, &time, .FLOAT)
	sun := 1 - night_amount()
	rl.SetShaderValue(water.shader, water.sun, &sun, .FLOAT)
	rl.BeginShaderMode(water.shader)
	// Altura negativa: desvira a textura (o OpenGL guarda de baixo para cima).
	rl.DrawTextureRec(tex, {0, 0, f32(tex.width), -f32(tex.height)}, {0, 0}, rl.WHITE)
	rl.EndShaderMode()
}
