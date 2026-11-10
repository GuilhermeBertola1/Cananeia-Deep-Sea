package game

// Sons gerados por código (v0.9). Nenhum arquivo de áudio: na hora de abrir
// o jogo, cada efeito é "sintetizado" num vetor de amostras com senos,
// ondas quadradas e ruído, e vira um rl.Sound.
//   - efeitos curtos (tiro, pesca, moeda, dano...): rl.Sound, tocam uma vez;
//   - fundo (mar e chuva): um WAV montado na memória e tocado como rl.Music
//     em loop, que a raylib emenda sem buraco.
// No navegador o som só começa depois do primeiro clique ou tecla: é regra
// do próprio navegador. A tecla N liga e desliga tudo.

import "core:math"
import rl "vendor:raylib"

SAMPLE_RATE :: 22050 // amostras por segundo: metade do CD, sobra para efeitos

Sfx :: enum {
	Shot, // nosso tiro
	Hit, // nosso tiro acertou alguma coisa
	Catch, // peixe no porão
	Coin, // dinheiro entrando
	Hurt, // levamos dano
	Item, // baú, arma, upgrade, encanto
	Alert, // evento, boss, clima: sino
	Sink, // um barco afundou
	Thunder, // relâmpago na tempestade
}

@(private = "file")
Audio :: struct {
	ready:      bool,
	muted:      bool,
	sounds:     [Sfx]rl.Sound,
	ocean:      rl.Music,
	rain:       rl.Music,
	ocean_wav:  []u8, // a Music lê destes bytes enquanto toca: não pode liberar
	rain_wav:   []u8,
	rain_vol:   f32,
	last_flash: bool,
	seed:       u32,
}

@(private = "file")
audio: Audio

sound_init :: proc() {
	rl.InitAudioDevice()
	if !rl.IsAudioDeviceReady() do return // sem placa de som: o jogo segue mudo
	audio.ready = true
	audio.seed = 0x1234567

	audio.sounds[.Shot] = make_shot()
	audio.sounds[.Hit] = make_hit()
	audio.sounds[.Catch] = make_catch()
	audio.sounds[.Coin] = make_coin()
	audio.sounds[.Hurt] = make_hurt()
	audio.sounds[.Item] = make_item()
	audio.sounds[.Alert] = make_alert()
	audio.sounds[.Sink] = make_sink()
	audio.sounds[.Thunder] = make_thunder()

	audio.ocean_wav = to_wav(make_ocean())
	audio.ocean = rl.LoadMusicStreamFromMemory(".wav", raw_data(audio.ocean_wav), i32(len(audio.ocean_wav)))
	audio.ocean.looping = true
	rl.SetMusicVolume(audio.ocean, 0.5)
	rl.PlayMusicStream(audio.ocean)

	audio.rain_wav = to_wav(make_rain())
	audio.rain = rl.LoadMusicStreamFromMemory(".wav", raw_data(audio.rain_wav), i32(len(audio.rain_wav)))
	audio.rain.looping = true
	rl.SetMusicVolume(audio.rain, 0)
	rl.PlayMusicStream(audio.rain)
}

sound_shutdown :: proc() {
	if !audio.ready do return
	for snd in audio.sounds do rl.UnloadSound(snd)
	rl.UnloadMusicStream(audio.ocean)
	rl.UnloadMusicStream(audio.rain)
	delete(audio.ocean_wav)
	delete(audio.rain_wav)
	rl.CloseAudioDevice()
}

// Toca um efeito, com a altura um pouco diferente a cada vez para não cansar.
play_sfx :: proc(fx: Sfx) {
	if !audio.ready do return
	snd := audio.sounds[fx]
	rl.SetSoundPitch(snd, 0.92 + f32(rl.GetRandomValue(0, 16)) / 100)
	rl.PlaySound(snd)
}

// Liga ou desliga todo o som (tecla N).
toggle_sound :: proc() -> (on: bool) {
	audio.muted = !audio.muted
	volume: f32 = 0 if audio.muted else 1
	rl.SetMasterVolume(volume)
	return !audio.muted
}

// Todo quadro: alimenta o mar e a chuva, e solta o trovão junto do relâmpago.
sound_update :: proc() {
	if !audio.ready do return
	rl.UpdateMusicStream(audio.ocean)
	rl.UpdateMusicStream(audio.rain)

	// A chuva soa com o clima de chuva e dentro (ou perto) da tempestade.
	in_storm := false
	if s.world.kind == WORLD_STORM {
		dx, dy := s.x - s.world.x, s.y - s.world.y
		reach := s.world.radius + 10
		in_storm = dx * dx + dy * dy <= reach * reach
	}
	want: f32 = 1 if s.weather == WEATHER_RAIN || in_storm else 0
	step := rl.GetFrameTime() / 3
	audio.rain_vol = min(audio.rain_vol + step, want) if audio.rain_vol < want else max(audio.rain_vol - step, want)
	rl.SetMusicVolume(audio.rain, audio.rain_vol * 0.45)

	// Trovão: no quadro em que o relâmpago acende.
	flash := in_storm && lightning()
	if flash && !audio.last_flash do play_sfx(.Thunder)
	audio.last_flash = flash
}

// --- síntese --------------------------------------------------------------------

@(private = "file")
ms :: proc(t: f32) -> int {
	return int(t * SAMPLE_RATE / 1000)
}

// Ruído branco de -1 a 1 (xorshift: rápido e sem estado global da raylib).
@(private = "file")
white :: proc() -> f32 {
	audio.seed ~= audio.seed << 13
	audio.seed ~= audio.seed >> 17
	audio.seed ~= audio.seed << 5
	return f32(audio.seed) / f32(max(u32)) * 2 - 1
}

// Tom de f0 a f1 Hz entre as amostras [from, to), com o volume caindo até 0.
// Os primeiros 2 ms sobem devagar para não estalar.
@(private = "file")
tone :: proc(buf: []f32, from, to: int, f0, f1, vol: f32, square := false) {
	phase: f32
	n := to - from
	for i in 0 ..< n {
		k := f32(i) / f32(n)
		phase += (f0 + (f1 - f0) * k) / SAMPLE_RATE
		v := math.sin(phase * math.TAU)
		if square do v = 1 if v > 0 else -1
		env := (1 - k) * (1 - k) * min(f32(i) / 44, 1)
		buf[from + i] += v * vol * env
	}
}

// Ruído filtrado: `smooth` perto de 0 deixa grave (trovão, mar), perto de 1 agudo (chiado).
@(private = "file")
noise :: proc(buf: []f32, from, to: int, vol, smooth: f32) {
	y: f32
	n := to - from
	for i in 0 ..< n {
		k := f32(i) / f32(n)
		y += (white() - y) * smooth
		buf[from + i] += y * vol * (1 - k) * (1 - k) * min(f32(i) / 44, 1)
	}
}

// Amostras (-1 a 1) para um rl.Sound de 16 bits.
@(private = "file")
to_sound :: proc(buf: []f32) -> rl.Sound {
	pcm := make([]i16, len(buf))
	defer delete(pcm)
	for v, i in buf do pcm[i] = i16(clamp(v, -1, 1) * 32000)
	wave := rl.Wave {
		frameCount = u32(len(pcm)),
		sampleRate = SAMPLE_RATE,
		sampleSize = 16,
		channels   = 1,
		data       = raw_data(pcm),
	}
	return rl.LoadSoundFromWave(wave) // copia as amostras: pcm pode ser liberado
}

// Monta um arquivo WAV inteiro na memória (cabeçalho de 44 bytes + amostras).
@(private = "file")
to_wav :: proc(buf: []f32) -> []u8 {
	defer delete(buf)
	size := len(buf) * 2
	out := make([]u8, 44 + size)
	put16 :: proc(b: []u8, at: int, v: u32) {b[at] = u8(v); b[at + 1] = u8(v >> 8)}
	put32 :: proc(b: []u8, at: int, v: u32) {put16(b, at, v); put16(b, at + 2, v >> 16)}
	copy(out[0:], "RIFF")
	put32(out, 4, u32(36 + size))
	copy(out[8:], "WAVEfmt ")
	put32(out, 16, 16) // tamanho do bloco fmt
	put16(out, 20, 1) // PCM
	put16(out, 22, 1) // mono
	put32(out, 24, SAMPLE_RATE)
	put32(out, 28, SAMPLE_RATE * 2) // bytes por segundo
	put16(out, 32, 2) // bytes por amostra
	put16(out, 34, 16) // bits
	copy(out[36:], "data")
	put32(out, 40, u32(size))
	for v, i in buf do put16(out, 44 + i * 2, u32(u16(i16(clamp(v, -1, 1) * 32000))))
	return out
}

@(private = "file")
make_shot :: proc() -> rl.Sound {
	buf := make([]f32, ms(350))
	defer delete(buf)
	noise(buf, 0, ms(250), 0.6, 0.35) // estouro
	tone(buf, 0, ms(300), 160, 50, 0.55) // tranco grave
	return to_sound(buf)
}

@(private = "file")
make_hit :: proc() -> rl.Sound {
	buf := make([]f32, ms(160))
	defer delete(buf)
	tone(buf, 0, ms(150), 300, 120, 0.5)
	noise(buf, 0, ms(80), 0.3, 0.5)
	return to_sound(buf)
}

@(private = "file")
make_catch :: proc() -> rl.Sound {
	buf := make([]f32, ms(250))
	defer delete(buf)
	tone(buf, 0, ms(120), 700, 250, 0.5) // plop
	noise(buf, ms(40), ms(240), 0.15, 0.6) // respingo
	return to_sound(buf)
}

@(private = "file")
make_coin :: proc() -> rl.Sound {
	buf := make([]f32, ms(400))
	defer delete(buf)
	tone(buf, 0, ms(90), 988, 988, 0.18, true) // si
	tone(buf, ms(80), ms(400), 1319, 1319, 0.18, true) // mi
	return to_sound(buf)
}

@(private = "file")
make_hurt :: proc() -> rl.Sound {
	buf := make([]f32, ms(300))
	defer delete(buf)
	tone(buf, 0, ms(300), 240, 80, 0.25, true)
	noise(buf, 0, ms(150), 0.3, 0.3)
	return to_sound(buf)
}

@(private = "file")
make_item :: proc() -> rl.Sound {
	buf := make([]f32, ms(520))
	defer delete(buf)
	notes := [4]f32{523, 659, 784, 1047} // dó, mi, sol, dó: arpejo
	for f, i in notes do tone(buf, ms(f32(i) * 90), ms(f32(i) * 90 + 250), f, f, 0.3)
	return to_sound(buf)
}

@(private = "file")
make_alert :: proc() -> rl.Sound {
	buf := make([]f32, ms(1200))
	defer delete(buf)
	// Sino: a nota e uns harmônicos "tortos" (não múltiplos inteiros), que é o que soa metálico.
	tone(buf, 0, len(buf), 880, 880, 0.3)
	tone(buf, 0, ms(700), 880 * 2.76, 880 * 2.76, 0.1)
	tone(buf, 0, ms(400), 880 * 5.4, 880 * 5.4, 0.05)
	return to_sound(buf)
}

@(private = "file")
make_sink :: proc() -> rl.Sound {
	buf := make([]f32, ms(1200))
	defer delete(buf)
	tone(buf, 0, len(buf), 200, 40, 0.5) // afundando
	noise(buf, 0, len(buf), 0.9, 0.08) // borbulhas
	return to_sound(buf)
}

@(private = "file")
make_thunder :: proc() -> rl.Sound {
	buf := make([]f32, ms(2500))
	defer delete(buf)
	noise(buf, 0, ms(150), 0.6, 0.3) // estalo
	noise(buf, 0, len(buf), 4.5, 0.02) // ronco grave
	return to_sound(buf)
}

// Mar: ruído grave que cresce e baixa como onda, 2 ondas em 8 s. O fim emenda
// com o começo porque a onda volta ao mesmo ponto.
@(private = "file")
make_ocean :: proc() -> []f32 {
	buf := make([]f32, SAMPLE_RATE * 8)
	low, high: f32
	for &v, i in buf {
		t := f32(i) / SAMPLE_RATE
		swell := math.sin(t * math.PI / 4)
		swell *= swell // 0 a 1, duas vezes em 8 s
		low += (white() - low) * 0.03
		high += (white() - high) * 0.2
		v = low * 3.5 * (0.35 + 0.65 * swell) + high * 0.15 * swell * swell * swell
	}
	return buf
}

// Chuva: chiado constante (sem emenda audível) e uns pingos mais fortes.
@(private = "file")
make_rain :: proc() -> []f32 {
	buf := make([]f32, SAMPLE_RATE * 4)
	y: f32
	for &v in buf {
		y += (white() - y) * 0.5
		v = y * 0.6
	}
	for _ in 0 ..< 60 {
		at := int(audio.seed % u32(len(buf) - ms(30)))
		noise(buf, at, at + ms(25), 0.5, 0.7)
	}
	return buf
}
