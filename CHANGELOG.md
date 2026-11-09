# Changelog

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
