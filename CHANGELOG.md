# Changelog

## v0.7.0 (2026-11-09): ecossistema, vilas, tubarões e eventos do mundo

- **Estoque de peixe por região:** o mar é dividido em regiões de 32x32 células. Pescar esvazia a região, a pesca rende menos quando ela está vazia e o peixe volta sozinho com o tempo (de vazia a cheia em uns 6 minutos). Cardumes aparecem mais onde tem peixe e vão embora quando a região cai abaixo de 20%
- **Vilas de pescadores** (`V`) em 3 ilhas, a 60–200 células do porto, sempre no mesmo lugar para o mesmo mapa. Cada vila compra peixe e conserta o barco (tecla E), paga 25% a mais pela espécie preferida dela e tem zona segura em volta
- **Tubarões** (`^`): até 10 no mar fundo, fora das zonas seguras. Caçam barcos fracos (vida abaixo de 120) e mordem 8 de vida a cada 1,5 s. Dá para afundar um com tiros e ganhar $15
- **Eventos do mundo**, um de cada vez, o primeiro entre 2 e 4 minutos e depois a cada 4 a 8 minutos, anunciados para todos:
  - **Tempestade:** anda com o vento, empurra o barco e tira vida aos poucos (só avaria, não afunda), mas a pesca rende em dobro
  - **Maré de lulas:** 6 cardumes de lula num ponto do mar fundo por 90 s
  - **Navio fantasma:** 300 de vida, persegue e atira em quem chegar perto. Ao afundar, divide $300 entre quem acertou, conforme o dano, e cada um ganha um item
- Tela: vilas no mapa e no minimapa, janela do lojista da vila, barra "Peixe na região" no painel, tubarões e navio fantasma com barra de vida, tempestade com chuva e relâmpagos, linha do evento no painel com tempo e direção, e o círculo do evento no minimapa
- Protocolo versão 7: estoque da região, bichos por perto e evento do mundo no `S_STATE`, flag "na vila", vilas no `S_MARKS`, eventos 33 a 37
- Os perfis da v0.6 continuam valendo sem conversão, porque o `profile_t` não mudou

## v0.6.0 (2026-11-09): sereia, mercenário e mecânico

- **Mercenário** no cais (NPC `M`, tecla B): fica com as bags dos outros e passa os peixes delas para o seu porão, até encher. Paga $25 por coroa, menos pela coroa que a sereia pediu
- **Sereia** numa pedra em mar aberto (`S`, a 70–110 células do porto, sempre no mesmo lugar para o mesmo mapa; zona segura em volta). Tecla E perto dela: roleta uma vez por minuto que pode roubar 10% do dinheiro (no máximo $100), dar a missão "derrote o jogador X" ou só cantar. O alvo fica sabendo que está sendo caçado
- Levar a coroa do alvo para a sereia **encanta** um item aleatório da bolsa: até +3, cada nível dá +25% (no mínimo +1) nos stats do item
- **Mecânico** no porto (NPC `C`, tecla C): junta dois itens do mesmo tipo num só, sorteado de novo pelo gerador. Sai no mínimo com a raridade do melhor; com raridades iguais, uma acima. O encanto maior fica. $80 a $600
- Tela: janelas do mercenário, da sereia e do mecânico, a sereia no mar e no minimapa, missão no painel e `+N` nos itens encantados
- Protocolo versão 6: `C_MERC`, `C_MERMAID`, `C_COMBINE`, eventos 20 a 32, flag "perto da sereia" no `S_STATE`, encanto e missão no `S_INVENTORY`, sereia no `S_MARKS`
- Perfis da v0.5 são convertidos sem perder nada (encantos e missão no fim do `profile_t`)

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
