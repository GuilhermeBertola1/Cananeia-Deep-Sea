# Roadmap: Cananeia Deep Sea

Cada versão entrega algo jogável. A ordem segue a seção 18 da especificação SCOM 3.

## Como fazer cada nova versão

1. Atualize o código: `git checkout main && git pull`.
2. Crie um branch para a versão: `git checkout -b v0.3-inventario`.
3. Um commit por passo, no padrão [Conventional Commits](https://www.conventionalcommits.org/pt-br/), em português: `tipo(escopo): descrição`. Exemplos: `feat(pesca): rede pega dois peixes por vez`, `fix(rede): ignorar mensagem curta`. Tipos: `feat`, `fix`, `refactor`, `docs`, `style`, `test`, `chore`, `perf`. Escopos comuns: `cliente`, `servidor`, `esp32`, `protocolo`, `mapa`, `pesca`, `loja`.
4. Se mudar mensagens da rede, atualize **os dois lados** (`protocol.h` e `protocol.odin`), o `docs/protocol.md` e aumente `PROTOCOL_VERSION`.
5. Se mudar a geração do mapa, mude `world.c` **e** `world.odin` juntos. O checksum no `S_WELCOME` acusa se ficaram diferentes.
6. Teste no PC com duas abas (`server\host\cananeia-host.exe`) e, quando der, no ESP32.
7. Escreva o que mudou no `CHANGELOG.md`.
8. Abra um Pull Request no GitHub. O Bertola revisa o código antes de qualquer merge na `main`.
9. Crie a tag da versão: `git tag v0.3.0 && git push --tags`.

## Visão geral

| Versão | Tema | Status |
| --- | --- | --- |
| v0.1 | Protótipo TypeScript (branch `prototipo-ts`) | Pronta |
| v0.2 | Nova base: Odin/WASM + ESP32-S3, mapa, rede binária, pesca, cardumes, lojista, ranking | Pronta |
| v0.3 | Contas com senha, inventário e itens procedurais | Próxima |
| v0.4 | Upgrades do barco | |
| v0.5 | Armas, PvP, morte e drops (bag e coroa) | |
| v0.6 | Sereia, mercenário e mecânico | |
| v0.7 | Ecossistema: cardumes dinâmicos e eventos do mundo | |
| v0.8 | Bosses (Cthulhu, Megalodon, Kraken) e armas únicas | |
| v0.9 | Shaders de água, luz e clima; polimento | |
| v1.0 | Lançamento | |

---

## v0.2: Nova base (pronta)

- [x] Renderização ASCII em camadas (oceano, ilhas, vida marinha, barcos, efeitos, céu)
- [x] Movimento do barco autoritativo no servidor
- [x] Oceano e ilhas gerados por semente, iguais no ESP32 e no WASM
- [x] Rede binária por WebSocket
- [x] Pesca passiva, cardumes, porão
- [x] Lojista com preço que varia com a oferta
- [x] Ranking por dinheiro (online e offline), perfis salvos por nome
- [x] Servidor no ESP32-S3 servindo o cliente pela flash

## v0.3: Contas, inventário e itens procedurais

**Objetivo:** cada item é único e muda o jeito de jogar; ninguém entra com o nome dos outros.

- [ ] Senha no perfil: `C_JOIN` manda a senha, o servidor guarda só o hash (PBKDF2 do mbedTLS no ESP32)
- [ ] Itens-base (arpão, rede, motor a vapor...) com faixa de stats, numa tabela em `components/game/items.c`
- [ ] Gerador procedural no servidor: raridade (comum 70%, raro 22%, épico 7%, lendário 1%) + 0 a 3 afixos; o item é guardado só como `semente + tipo` (pouca memória)
- [ ] Inventário com slots: 1 arma, 1 ferramenta de pesca, 3 boosts
- [ ] Stats somados no barco: velocidade, vida, dano, produção, sorte
- [ ] Mensagens novas: `S_INVENTORY`, `C_EQUIP`
- [ ] Tela de inventário (tecla I) no cliente

**Pronta quando:** dois jogadores com itens diferentes pescam em ritmos visivelmente diferentes.

## v0.4: Upgrades do barco

- [ ] **Vela:** velocidade máxima
- [ ] **Casco:** vida e capacidade do porão
- [ ] **Leme:** direção com inércia; leme melhor vira mais rápido
- [ ] **Durabilidade:** bater na costa gasta; reparo pago no porto
- [ ] NPC **Estaleiro** no porto com níveis e preços
- [ ] Barco desenhado diferente conforme o nível

## v0.5: Armas, PvP, morte e drops

- [ ] Vida do barco e barra de vida no HUD
- [ ] Tiro com a arma equipada, alcance e recarga; dano só no servidor
- [ ] Ao morrer: cai uma **bag** (peixes do porão) e uma **coroa**, renasce no porto
- [ ] Zonas seguras perto das ilhas com NPC
- [ ] Bags e coroas no minimapa (`S_MARKS`)

## v0.6: Sereia, mercenário e mecânico

- [ ] **Mercenário:** entregar a bag de outro jogador dá os peixes dela para você
- [ ] **Sereia:** roleta da sorte que pode roubar ou dar a missão "derrote o jogador X"; levar a coroa do alvo encanta um item aleatório
- [ ] **Mecânico:** combina dois itens do mesmo tipo usando o gerador procedural

## v0.7: Ecossistema vivo

- [ ] Cardumes dinâmicos: população por região, pesca demais esvazia, recupera com o tempo
- [ ] Predadores que atacam barcos fracos
- [ ] Eventos do mundo: tempestade, maré de lulas, navio fantasma
- [ ] Mais ilhas com NPCs

## v0.8: Bosses

- [ ] Cthulhu, Megalodon e Kraken aparecem como eventos anunciados para todos
- [ ] Vida compartilhada; dano de vários jogadores
- [ ] Drop de dinheiro + arma única de cada boss

## v0.9: Shaders e polimento

- [ ] Shader de água (ondas e reflexo) por cima da camada do oceano
- [ ] Luz: lanterna do barco, faróis nas ilhas
- [ ] Clima: chuva e neblina
- [ ] Sons gerados por código

## v1.0: Lançamento

- [ ] Teste com 8 jogadores no ESP32 por 1 hora sem travar
- [ ] OTA: atualizar o ESP32 pelo Wi-Fi
- [ ] Página do jogo no GitHub com GIF
