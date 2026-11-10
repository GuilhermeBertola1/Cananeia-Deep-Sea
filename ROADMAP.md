# Roadmap: Cananeia Deep Sea

Cada versão entrega algo jogável. A ordem segue a seção 18 da especificação SCOM 3.

## Como fazer cada nova versão

1. Atualize o código: `git checkout main && git pull`.
2. Crie um branch para a versão: `git checkout -b v1.1-nome-da-versao`.
3. Um commit por passo, no padrão [Conventional Commits](https://www.conventionalcommits.org/pt-br/), em português: `tipo(escopo): descrição`. Exemplos: `feat(pesca): rede pega dois peixes por vez`, `fix(rede): ignorar mensagem curta`. Tipos: `feat`, `fix`, `refactor`, `docs`, `style`, `test`, `chore`, `perf`. Escopos comuns: `cliente`, `servidor`, `esp32`, `protocolo`, `mapa`, `pesca`, `loja`.
4. Se mudar mensagens da rede, atualize **os dois lados** (`protocol.h` e `protocol.odin`), o `docs/protocol.md` e aumente `PROTOCOL_VERSION`.
5. Se mudar a geração do mapa, mude `world.c` **e** `world.odin` juntos. O checksum no `S_WELCOME` acusa se ficaram diferentes.
6. Teste no PC com duas abas (`server\host\cananeia-host.exe`) e, quando der, no ESP32. Antes de lançar, rode os robôs (`server\host\bots.exe`, veja o README) por 1 hora no ESP32.
7. Escreva o que mudou no `CHANGELOG.md`.
8. Abra um Pull Request no GitHub. O Bertola revisa o código antes de qualquer merge na `main`.
9. Crie a tag da versão: `git tag v1.1.0 && git push --tags`.
10. Campo novo no `profile_t` vai sempre no fim, e o `profile_upgrade` em `storage.h` aprende a converter o perfil da versão anterior. Assim ninguém perde a conta.

## Visão geral

| Versão | Tema | Status |
| --- | --- | --- |
| v0.1 | Protótipo TypeScript (branch `prototipo-ts`) | Pronta |
| v0.2 | Nova base: Odin/WASM + ESP32-S3, mapa, rede binária, pesca, cardumes, lojista, ranking | Pronta |
| v0.3 | Contas com senha, inventário e itens procedurais | Pronta |
| v0.4 | Upgrades do barco | Pronta |
| v0.5 | Armas, PvP, morte e drops (bag e coroa) | Pronta |
| v0.6 | Sereia, mercenário e mecânico | Pronta |
| v0.7 | Ecossistema: cardumes dinâmicos e eventos do mundo | Pronta |
| v0.8 | Bosses (Cthulhu, Megalodon, Kraken) e armas únicas | Pronta |
| v0.9 | Shaders de água, luz e clima; polimento | Pronta |
| v1.0 | Lançamento: 8 jogadores no ESP32 | Pronta |

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

## v0.3: Contas, inventário e itens procedurais (pronta)

**Objetivo:** cada item é único e muda o jeito de jogar; ninguém entra com o nome dos outros.

- [x] Senha no perfil: `C_JOIN` manda a senha, o servidor guarda só o hash (PBKDF2 do mbedTLS no ESP32)
- [x] Itens-base (arpão, rede, motor a vapor...) com faixa de stats, numa tabela em `components/game/items.c`
- [x] Gerador procedural no servidor: raridade (comum 70%, raro 22%, épico 7%, lendário 1%) + 0 a 3 afixos; o item é guardado só como `semente + tipo` (pouca memória)
- [x] Inventário com slots: 1 arma, 1 ferramenta de pesca, 3 boosts
- [x] Stats somados no barco: velocidade, vida, dano, produção, sorte
- [x] Mensagens novas: `S_INVENTORY`, `C_EQUIP`, `C_DISCARD`
- [x] Tela de inventário (tecla I) no cliente
- [x] Baú com item aleatório na pesca (sorte aumenta a chance)

**Pronta quando:** dois jogadores com itens diferentes pescam em ritmos visivelmente diferentes.

## v0.4: Upgrades do barco (pronta)

**Objetivo:** dinheiro vira barco melhor, e o barco melhor muda o jeito de navegar.

- [x] **Vela:** velocidade máxima (+1 a +5)
- [x] **Casco:** vida e capacidade do porão (150 a 420)
- [x] **Leme:** direção com inércia; leme melhor vira mais rápido (90°/s a 450°/s)
- [x] **Durabilidade:** é a vida do barco; bater na costa gasta, reparo pago no porto; vida 0 = avariado
- [x] NPC **Estaleiro** no porto com níveis e preços (`server/components/game/boat.c`)
- [x] Barco desenhado diferente conforme o nível do casco e da vela
- [x] Perfis da v0.3 convertidos, sem perder contas

**Pronta quando:** um barco com vela e leme no máximo dá a volta na ilha do porto visivelmente mais rápido que um barco novo.

## v0.5: Armas, PvP, morte e drops (pronta)

**Objetivo:** o mar fica perigoso: quem pesca muito longe do porto pode perder o porão.

- [x] Tiros tiram vida do barco (a vida e a barra no HUD já existem desde a v0.4)
- [x] Tiro com a arma equipada, alcance e recarga; dano só no servidor (`server/components/game/weapon.c`)
- [x] Vida 0 por tiro = morte (na costa continua sendo só avaria). Ao morrer: cai uma **bag** (peixes do porão) e uma **coroa**, renasce no porto
- [x] Zonas seguras perto das ilhas com NPC (por enquanto, o porto)
- [x] Bags e coroas no minimapa (`S_MARKS`), saque guardado no barco (até 4) e abates/mortes no perfil
- [x] Perfis da v0.4 convertidos, sem perder contas

**Pronta quando:** dois jogadores fora da zona segura trocam tiros, um afunda, e o outro pega a bag e a coroa que caíram.

## v0.6: Sereia, mercenário e mecânico (pronta)

**Objetivo:** dar uso para o saque do PvP e um jeito de melhorar os itens.

- [x] **Mercenário:** entregar a bag de outro jogador dá os peixes dela para você (e as coroas viram recompensa)
- [x] **Sereia:** roleta da sorte que pode roubar ou dar a missão "derrote o jogador X"; levar a coroa do alvo encanta um item aleatório
- [x] **Mecânico:** combina dois itens do mesmo tipo usando o gerador procedural
- [x] A sereia mora numa pedra em mar aberto, com zona segura em volta
- [x] Perfis da v0.5 convertidos, sem perder contas

**Pronta quando:** um jogador afunda o alvo da missão, leva a coroa para a sereia e vê o item encantado no inventário; e o mecânico junta dois itens iguais num de raridade maior.

## v0.7: Ecossistema vivo (pronta)

**Objetivo:** o mar muda sozinho: pescar sempre no mesmo lugar não compensa e sair para longe tem riscos e prêmios.

- [x] Cardumes dinâmicos: estoque de peixe por região (32x32 células), pesca demais esvazia, recupera com o tempo
- [x] Predadores: tubarões que caçam barcos fracos fora das zonas seguras
- [x] Eventos do mundo: tempestade, maré de lulas, navio fantasma (recompensa dividida pelo dano)
- [x] Mais ilhas com NPCs: 3 vilas de pescadores que compram peixe e consertam o barco
- [x] Perfis da v0.6 continuam valendo (o `profile_t` não mudou)

**Pronta quando:** pescar muito numa região baixa a barra de peixe e ela volta com o tempo; um barco novo longe do porto é caçado por tubarões; e um grupo afunda o navio fantasma e divide o tesouro.

## v0.8: Bosses (pronta)

**Objetivo:** de tempos em tempos o servidor inteiro tem um motivo para se juntar.

- [x] Cthulhu, Megalodon e Kraken aparecem como eventos anunciados para todos, numa agenda separada dos eventos do mundo
- [x] Cada um com um ataque próprio: investida (Megalodon), tentáculos que agarram e puxam (Kraken), anel de tiros e loucura que inverte o leme (Cthulhu)
- [x] Vida compartilhada que cresce com os jogadores online; dano de vários jogadores somado
- [x] Drop de dinheiro dividido pelo dano + arma única de cada boss para quem mais bateu
- [x] Armas únicas com poder especial: Dente (conserta o barco), Tentáculo (prende), Cajado (tiro em leque)
- [x] Perfis da v0.7 continuam valendo (o `profile_t` não mudou)

**Pronta quando:** dois jogadores afundam um boss juntos, os dois veem a mesma barra de vida caindo, o prêmio sai dividido pelo dano e quem bateu mais encontra a arma única no inventário.

## v0.9: Shaders e polimento (pronta)

- [x] Shader de água (ondas e reflexo) por cima da camada do oceano
- [x] Dia e noite pelo relógio do servidor, igual para todos
- [x] Luz: lanterna do barco, faróis no porto e nas vilas
- [x] Clima: chuva (pesca +25%) e neblina (o servidor esconde quem está longe)
- [x] Sons gerados por código, com tecla para desligar
- [x] Perfis da v0.8 continuam valendo (o `profile_t` não mudou)

**Pronta quando:** com duas abas abertas, as duas anoitecem juntas e veem o facho do farol girando; na neblina, cada barco some da tela do outro ao passar de 16 células; e o tiro, a pesca e a venda têm som.

## v1.0: Lançamento (pronta)

**Objetivo:** o ESP32 aguenta a turma toda jogando junto.

- [x] Servidor HTTP do ESP32 sem derrubar quem está parado, com envio que não trava o jogo e keep-alive para quem some do Wi-Fi
- [x] Rota `/status` com a saúde do servidor (memória, tick mais lento, quedas), também no monitor serial a cada minuto
- [x] Robôs para o teste de carga (`server/host/bots.c`)
- [x] Teste com 8 jogadores no ESP32 por 1 hora sem travar
- [x] Perfis da v0.9 continuam valendo (o `profile_t` não mudou)

**Pronta quando:** `bots.exe` com 8 robôs no ESP32 por 60 minutos termina com `PASSOU`, e o `heap_min` do `/status` para de cair depois dos primeiros minutos.
