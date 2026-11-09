# Cananeia Deep Sea

Jogo online em ASCII: você navega um barco por um oceano enorme com ilhas, pesca enquanto anda, caça cardumes, vende no porto e disputa o ranking de dinheiro com os outros capitães.

Segue a especificação **SCOM 3**:

- **Cliente:** [Odin](https://odin-lang.org) + [raylib](https://www.raylib.com), compilado para **WebAssembly**. Só caracteres ASCII, sem texturas externas, desenhado em camadas (oceano, ilhas, vida marinha, barcos, efeitos, céu).
- **Servidor:** **ESP32-S3** (8 MB de PSRAM, 16 MB de flash) com **ESP-IDF 6.1**. Ele é a autoridade do jogo e também serve a página do cliente para quem se conecta.
- **Rede:** WebSocket com mensagens binárias compactas ([docs/protocol.md](docs/protocol.md)).
- **Mapa:** gerado pela semente dos dois lados, só com inteiros, então o ESP32 e o navegador chegam no mesmo oceano sem mandar o mapa pela rede.

O protótipo antigo em TypeScript está no branch `prototipo-ts`.

## Como jogar

| Tecla | Ação |
| --- | --- |
| WASD / setas | navegar |
| E | falar com o lojista `L` (no cais `H`) |
| 1 a 9 / T | vender uma espécie / vender tudo (na loja) |
| U | estaleiro `E` (no cais): 1 vela, 2 casco, 3 leme, R conserta |
| Clique / Espaço | atirar na mira `+` (com uma arma equipada) |
| M | mapa grande |
| I | inventário: W/S escolhe, Enter equipa ou tira, X duas vezes joga fora |
| Esc | fechar janela |

Você pesca sozinho enquanto navega. Dentro de um cardume (`><`) pesca muito mais. Mar fundo paga melhor. O preço cai quando muita gente vende a mesma espécie e volta com o tempo. Às vezes vem um baú com um item: rede melhor pesca mais rápido, motor deixa o barco mais veloz, amuleto traz mais baús. Cada capitão entra com nome e senha.

O barco tem inércia: acelera aos poucos, vira 45° por vez e desliza quando você solta a tecla. Bater na costa rápido tira vida; com vida 0 ele fica avariado e lento até você consertar. No estaleiro, o dinheiro vira vela (velocidade), casco (vida e porão) e leme (vira mais rápido), e o barco muda de desenho.

Com um arpão, mosquete ou canhão equipado, você atira onde o mouse aponta. Perto do porto é zona segura. Barco que zera a vida por tiro afunda: deixa no mar uma bag (`$`) com os peixes do porão e uma coroa (`W`), e o capitão volta ao porto. Passe por cima para pegar: a sua bag devolve os peixes, e o saque dos outros fica guardado no barco.

## Estrutura

```
client/                 cliente (Odin + raylib -> WASM)
  src/                  pacote do jogo: mundo, rede, desenho, HUD
  src/main_web/         entrada da versão web (navegador)
  src/main_desktop/     entrada desktop (modo offline, para testar o visual)
  web/index.html        página que carrega o WASM
  build_web.bat / .sh   compila para build/web
server/                 servidor
  main/                 ESP32-S3: Wi-Fi, HTTP, WebSocket, tarefa do jogo
  components/game/      núcleo do jogo em C (igual no ESP32 e no PC)
  host/                 servidor de teste para o PC (mesmo núcleo)
  partitions.csv        app 4 MB (com o cliente web dentro), perfis 1 MB
docs/
  protocol.md           mensagens da rede
  instalacao.md         como instalar Odin, emsdk e ESP-IDF no Windows
```

## Rodando

Instalação das ferramentas: [docs/instalacao.md](docs/instalacao.md).

**No PC (teste rápido):**

```bat
client\build_web.bat
server\host\build.bat
server\host\cananeia-host.exe client\build\web 8080
```

Abra http://localhost:8080 em duas abas.

**No ESP32-S3:**

```bat
client\build_web.bat
cd server
idf.py set-target esp32s3
idf.py -p COM5 flash monitor
```

Conecte na rede Wi-Fi `CananeiaDeepSea` (senha `cananeia123`) e abra http://192.168.4.1. Para usar a rede da sua casa: `idf.py menuconfig` → *Cananeia Deep Sea*.

## Contribuindo

Veja o [ROADMAP.md](ROADMAP.md). Commits no padrão Conventional Commits, em português, e toda mudança passa pela revisão do Bertola antes de entrar na `main`.
