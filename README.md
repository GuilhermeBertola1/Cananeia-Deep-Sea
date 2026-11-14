# Cananeia Deep Sea

Jogo online em ASCII: você navega um barco por um oceano enorme com ilhas, pesca enquanto anda, caça cardumes, vende no porto e disputa o ranking de dinheiro com os outros capitães.

Segue a especificação **SCOM 3**:

- **Cliente:** [Odin](https://odin-lang.org) + [raylib](https://www.raylib.com), compilado para **WebAssembly**. Só caracteres ASCII, sem texturas externas, desenhado em camadas (oceano, ilhas, vida marinha, barcos, efeitos, céu).
- **Servidor:** o servidor do PC, um núcleo em C que é a autoridade do jogo e também serve a página do cliente para quem se conecta. Até a v1.1 ele também rodava num **ESP32-S3** (8 MB de PSRAM) com **ESP-IDF 6.1**; a partir da v1.2 o mundo grande precisa de mais memória, e o firmware fica só como referência.
- **Rede:** WebSocket com mensagens binárias compactas ([docs/protocol.md](docs/protocol.md)).
- **Mapa:** um mundo de 65536 × 65536 gerado pela semente dos dois lados, só com inteiros, e só perto dos barcos (em chunks de 64 × 64). O servidor e o navegador chegam no mesmo oceano sem mandar o mapa pela rede.

O protótipo antigo em TypeScript está no branch `prototipo-ts`.

## Como jogar

| Tecla | Ação |
| --- | --- |
| WASD / setas | navegar |
| E | falar com o lojista `L` (no cais `H`), com o lojista de uma vila `V` ou com a sereia `S` (no mar) |
| 1 a 9 / T | vender uma espécie / vender tudo (na loja) |
| U | estaleiro `E` (no cais): 1 vela, 2 casco, 3 leme, R conserta |
| R | na loja de uma vila: consertar o barco |
| B | mercenário `M` (no cais): T entrega as bags e coroas |
| C | mecânico `C` (no porto): Enter marca um item, Enter no segundo junta os dois |
| F | armeiro `A` (no forte): 1 a 6 compram uma arma. Mascate `K` (numa vila): 1 e 2 compram, G entrega a encomenda |
| G / X | na sereia: girar a roleta ou entregar a coroa / desistir da missão |
| Clique / Espaço | atirar na mira `+` (com uma arma equipada) |
| M | mapa grande: clique marca um waypoint (clique em cima apaga), 1 a 8 escolhem qual seguir |
| N | liga e desliga o som |
| I | inventário: W/S escolhe, Enter equipa ou tira, X duas vezes joga fora |
| Esc | fechar janela |

Você pesca sozinho enquanto navega. Dentro de um cardume (`><`) pesca muito mais. Mar fundo paga melhor. Cada balcão (o porto e cada vila) tem o seu preço: peixe que sobra por ali vale menos, peixe raro vale mais, e o preço cai quando muita gente vende a mesma espécie no mesmo balcão (e um pouco nos balcões vizinhos), voltando com o tempo. Às vezes vem um baú com um item: rede melhor pesca mais rápido, motor deixa o barco mais veloz, amuleto traz mais baús. Cada capitão entra com nome e senha.

O barco tem inércia: acelera aos poucos, vira 45° por vez e desliza quando você solta a tecla. Bater na costa rápido tira vida; com vida 0 ele fica avariado e lento até você consertar. No estaleiro, o dinheiro vira vela (velocidade), casco (vida e porão) e leme (vira mais rápido), e o barco muda de desenho.

Com um arpão, mosquete ou canhão equipado, você atira onde o mouse aponta. Perto do porto é zona segura. Barco que zera a vida por tiro afunda: deixa no mar uma bag (`$`) com os peixes do porão e uma coroa (`W`), e o capitão volta ao porto. Passe por cima para pegar: a sua bag devolve os peixes, e o saque dos outros fica guardado no barco.

No porto, o mercenário troca as bags dos outros pelos peixes delas e paga uma recompensa pelas coroas. Em mar aberto, numa pedra, mora a sereia: a roleta dela pode te roubar ou dar a missão de afundar outro capitão; trazendo a coroa dele, ela encanta um dos seus itens. O mecânico junta dois itens do mesmo tipo num melhor.

O mar é enorme e muda com a distância do porto: a Baía em volta dele, depois mangues, arquipélagos, recifes, mar aberto e, bem longe, o abismo, cada um com os seus peixes. As vilas e as sereias só aparecem no mapa quando você chega perto, então vale marcar no mapa grande os lugares que achar: os waypoints ficam salvos e só você vê.

O mar está vivo. Cada região tem uma quantidade de peixe: pesque demais no mesmo lugar e ela esvazia (o painel mostra quanto sobrou), depois se recupera sozinha. Em outras ilhas há vilas de pescadores (`V`) que compram peixe, pagam mais pela espécie preferida e consertam o barco. Tubarões (`^`) rondam o mar fundo atrás de barcos fracos. De tempos em tempos acontece um evento para todos: uma tempestade que empurra o barco mas dobra a pesca, uma maré de lulas, ou um navio fantasma que atira em quem chegar perto e divide o tesouro entre quem ajudou a afundá-lo.

E às vezes sobe um boss, anunciado para todos, com uma vida só para o servidor inteiro: o Megalodon, que dá investidas no mar fundo; o Kraken, que agarra e puxa quem chega perto; ou o Cthulhu, que atira em volta e enlouquece o leme de quem se aproxima. O dinheiro é dividido pelo dano, e quem mais bateu leva a arma única do boss: o Dente que conserta o barco, o Tentáculo que prende o alvo ou o Cajado que atira em leque.

Arma também se compra: num forte numa ilha perto do porto fica o armeiro (`A`), com arpão, mosquete e canhão em comum ou raro. Em cada vila mora um mascate (`K`) que vende itens épicos já encantados, um de cada (quem chegar primeiro leva), e encomenda peixe pagando o dobro do mercado. A mercadoria dele troca a cada 30 minutos, em todas as vilas juntas.

O mar balança, o dia vira noite na mesma hora para todo mundo e, no escuro, só se enxerga até onde vai a lanterna do barco e o facho dos faróis. O clima muda para todos: a chuva faz o peixe subir, e a neblina esconde quem está a mais de 16 células. Tudo tem som, gerado pelo próprio jogo (N desliga).

## Estrutura

```
client/                 cliente (Odin + raylib -> WASM)
  src/                  pacote do jogo: mundo, rede, desenho, HUD, shaders e sons
  src/main_web/         entrada da versão web (navegador)
  src/main_desktop/     entrada desktop (modo offline, para testar o visual)
  web/index.html        página que carrega o WASM
  build_web.bat / .sh   compila para build/web
server/                 servidor
  main/                 ESP32-S3 (até a v1.1): Wi-Fi, HTTP, WebSocket, tarefa do jogo
  components/game/      núcleo do jogo em C: mundo, chunks, peixes, itens, jogo
  host/                 servidor do PC, robôs do teste de carga, mapa.c (desenha o mapa) e test_mapa.c
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

**No ESP32-S3 (só até a v1.1):**

```bat
client\build_web.bat
cd server
idf.py set-target esp32s3
idf.py -p COM5 flash monitor
```

Conecte na rede Wi-Fi `CananeiaDeepSea` (senha `cananeia123`) e abra http://192.168.4.1. Para usar a rede da sua casa: `idf.py menuconfig` → *Cananeia Deep Sea*.

**Teste de carga com robôs:**

O `server\host\build.bat` também compila o `bots.exe`, que coloca jogadores falsos (`robo1`, `robo2`..., senha `robo1234`) para navegar, atirar e vender sozinhos. Metade deles é exploradora e navega reto para longe do porto, para o mundo grande trabalhar:

```bat
server\host\bots.exe 127.0.0.1 8080 8 5
server\host\bots.exe 192.168.4.1 80 8 60
```

O primeiro testa o servidor do PC com 8 robôs por 5 minutos; o segundo, o ESP32 por 1 hora (é o padrão, então `bots.exe` sozinho faz o mesmo). A cada minuto ele mostra quantos estão jogando, as quedas, o robô mais longe do porto e o `/status` do servidor (com `chunks`, quantos pedaços do mapa estão na memória), e no fim diz `PASSOU` ou `NÃO PASSOU`.

A saúde do servidor fica em http://192.168.4.1/status (ou http://localhost:8080/status no PC): tempo ligado, jogadores, memória livre (`heap_free`) e a menor até agora (`heap_min`), tick médio e mais lento, ticks atrasados e envios que falharam. No ESP32 a mesma linha sai no monitor serial a cada minuto.

## Contribuindo

Veja o [ROADMAP.md](ROADMAP.md). Commits no padrão Conventional Commits, em português, e toda mudança passa pela revisão do Bertola antes de entrar na `main`.
