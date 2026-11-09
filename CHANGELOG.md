# Changelog

## v0.5.0 (2026-11-09): armas, PvP, morte e drops

- Tiro com a arma equipada: mire com o mouse e clique (ou Espaço); segurando, atira de novo quando a recarga acaba. O servidor cria o tiro e decide quem acertou
- Arpão (alcance 8, recarga 0,6 s), mosquete (14, 1,2 s) e canhão leve (18, 2 s); o dano é o do item, então a raridade e o afixo Afiado contam (`server/components/game/weapon.c`)
- O tiro voa célula por célula e dá para desviar; some ao acertar, na terra ou no fim do alcance
- Zona segura a até 21 células do porto: ninguém atira e tiro não tira vida
- Vida 0 por tiro = morte (bater na costa continua sendo só avaria). Cai uma **bag** com os peixes do porão e uma **coroa**, e o capitão renasce no porto com o barco inteiro
- Passar por cima pega o saque: a sua bag devolve os peixes; a bag e a coroa dos outros ficam no barco (até 4) para a v0.6. Saque no mar some em 5 minutos
- Tela: tiros `*`, bags `$` e coroas `W` no mar e no minimapa, mira `+` vermelha no alcance, barrinha de vida nos outros barcos, arma e recarga no painel, saque e abates/mortes no inventário
- Protocolo versão 5: `C_FIRE`, recarga, zona segura, vida dos outros barcos e tiros no `S_STATE`, saque e placar no `S_INVENTORY`, bags e coroas no `S_MARKS`
- Perfis da v0.4 são convertidos sem perder nada (saque e abates/mortes no fim do `profile_t`)

## v0.4.0 (2026-11-08): upgrades do barco

- Estaleiro no cais (NPC `E`, tecla U): vela, casco e leme com 5 níveis cada, de $150 a $2500
- Vela aumenta a velocidade; casco aumenta a vida e o porão (150 a 420 peixes); leme vira mais rápido
- Leme com inércia: o barco acelera, vira 45° por vez e desliza um pouco depois que você solta a tecla
- Vida do barco: bater na costa rápido (4 células/s ou mais) gasta; o cais não estraga. Com vida 0 o barco fica avariado e anda devagar até o reparo ($0,20 por ponto)
- Barra de vida e velocidade no painel
- O barco muda de desenho conforme o nível do casco e da vela, para você e para os outros
- Protocolo versão 4: `C_UPGRADE`, `C_REPAIR`, `S_BOAT`, vida e velocidade atual no `S_STATE`
- Perfis da v0.3 são convertidos em vez de apagados (campos novos sempre no fim do `profile_t`)

## v0.3.0 (2026-10-26): contas, inventário e itens procedurais

- Senha no perfil: o servidor guarda só sal + hash PBKDF2-SHA256 (mbedTLS/PSA no ESP32, C puro no PC, conferidos com o RFC 7914)
- Tela de entrada com campo de senha; nome novo cria a conta
- Itens procedurais: 10 itens-base, raridade (comum 70%, raro 22%, épico 7%, lendário 1%) e até 3 afixos; o item é salvo só como semente + tipo
- Bolsa com 12 itens e slots no barco: 1 arma, 1 ferramenta de pesca, 3 boosts
- Stats somados no barco: velocidade, vida, dano, produção (chance de pescar) e sorte (chance de baú)
- Baú com item aleatório ao pescar; capitão novo começa com uma vara de bambu
- Tela de inventário (tecla I): equipar, desequipar e jogar fora
- Protocolo versão 3: `C_JOIN` com senha, `C_EQUIP`, `C_DISCARD`, `S_INVENTORY`
- `perfis.bin` agora tem cabeçalho; arquivo de versão antiga é descartado

## v0.2.0 (2026-10-23): nova base em Odin + ESP32-S3

Reescrita seguindo a especificação SCOM 3. O protótipo TypeScript foi para o branch `prototipo-ts`.

- Cliente em Odin + raylib compilado para WebAssembly, desenhado em camadas ASCII (oceano, ilhas, vida marinha, barcos, efeitos, céu) sem texturas externas
- Céu com nuvens e ciclo de dia e noite; à noite só o entorno do barco fica claro
- Servidor autoritativo em C para ESP32-S3 (ESP-IDF 6.1): Wi-Fi, HTTP e WebSocket, serve o cliente da flash, mapa na PSRAM
- Servidor de teste para o PC usando o mesmo núcleo em C
- Protocolo binário compacto (docs/protocol.md)
- Mapa 512 x 512 gerado só com inteiros: cliente e servidor geram o mesmo oceano e conferem pelo checksum
- Pesca passiva por bioma, cardumes que migram, lojista no cais com preço que varia com a oferta
- Ranking com jogadores offline e perfis salvos (NVS no ESP32, arquivo no PC)
- Minimapa com cardumes e cardume mais próximo, mapa grande (M)

## v0.1.0 (2026-10-23): protótipo TypeScript

- Oceano, barcos online, pesca, cardumes, venda e ranking em Node + WebSocket (branch `prototipo-ts`)
