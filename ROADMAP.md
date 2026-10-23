# Roadmap: Cananeia Deep Sea

Cada versão entrega algo jogável. Só comece a próxima quando a atual estiver divertida e sem bugs graves.

## Como fazer cada nova versão

1. Atualize o código: `git checkout main && git pull`.
2. Crie um branch para a versão: `git checkout -b v0.2-contas`.
3. Implemente os passos da versão, um commit por passo (`git commit -m "v0.2: salvar dinheiro no banco"`).
4. Teste com pelo menos duas abas abertas (`npm run dev`) e rode `npm run typecheck`.
5. Atualize o `version` no `package.json` e escreva o que mudou no `CHANGELOG.md`.
6. Abra um Pull Request no GitHub, revise e faça o merge na `main`.
7. Crie a tag da versão: `git tag v0.2.0 && git push --tags`.

## Visão geral

| Versão | Tema | Status |
| --- | --- | --- |
| v0.1 | Base: mapa, barcos online, pesca, cardumes, venda, ranking, minimapa | Pronta |
| v0.2 | Contas, salvamento e lojista | Próxima |
| v0.3 | Itens procedurais e equipamento | |
| v0.4 | Upgrades do barco | |
| v0.5 | PvP, bag, coroa e mercenário | |
| v0.6 | Sereia e mecânico | |
| v0.7 | Ecossistema vivo e mapa maior | |
| v0.8 | Bosses: Cthulhu, Megalodon e Kraken | |
| v0.9 | Servidor online e anti-trapaça | |
| v1.0 | Lançamento | |

---

## v0.1: Base (pronta)

- [x] Oceano 512 x 512 gerado por seed, com ilha-porto no centro
- [x] Servidor autoritativo em Node + WebSocket, 10 ticks por segundo
- [x] Barcos de vários jogadores em tempo real
- [x] Pesca passiva por bioma e cardumes que migram
- [x] Porão com capacidade, venda no cais, ranking por dinheiro
- [x] Minimapa com cardumes, porto e objetivo mais próximo

## v0.2: Contas, salvamento e lojista

**Objetivo:** o progresso do jogador não some quando ele fecha a aba.

- [ ] Adicionar banco SQLite (`better-sqlite3`) com tabela `players` (nome, hash da senha, dinheiro, porão, posição)
- [ ] Tela de login e cadastro (nome + senha, senha com `bcrypt`)
- [ ] Servidor devolve um token de sessão; o cliente guarda e reconecta sozinho se cair
- [ ] Salvar o jogador a cada 30 s e ao desconectar
- [ ] Ranking passa a incluir jogadores offline (top 10 do banco)
- [ ] NPC **Lojista** (`L`) na ilha-porto: chegar perto e apertar E abre uma janela de venda
- [ ] Preço varia com a oferta: muita gente vendendo a mesma espécie baixa o preço por alguns minutos

**Arquivos:** novo `server/db.ts`, `server/index.ts` (login), `server/game.ts` (salvar, lojista), `client/main.ts` (telas).

**Pronta quando:** fechar o navegador, voltar e o dinheiro e o porão continuam lá.

## v0.3: Itens procedurais e equipamento

**Objetivo:** cada item é único e muda o jeito de jogar.

- [ ] Criar `shared/items.ts` com os itens-base (arpão, rede, motor a vapor...) e a faixa de stats de cada um
- [ ] Gerador procedural no servidor: raridade (comum 70%, raro 22%, épico 7%, lendário 1%) + 0 a 3 afixos ("Afiado", "das Profundezas")
- [ ] Inventário com slots: 1 arma, 1 ferramenta de pesca, 3 boosts
- [ ] Stats somados no barco: velocidade, vida, dano, produção, sorte
- [ ] Ferramenta de pesca muda a chance e as espécies que dá para pegar
- [ ] Itens caem raramente ao pescar e podem ser comprados no porto
- [ ] Tela de inventário (tecla I) com comparação de stats
- [ ] Salvar itens no banco (tabela `items`)

**Pronta quando:** dois jogadores com itens diferentes pescam em ritmos visivelmente diferentes.

## v0.4: Upgrades do barco

**Objetivo:** dar um motivo para gastar dinheiro além dos itens.

- [ ] **Vela:** aumenta a velocidade máxima
- [ ] **Casco:** aumenta vida e capacidade do porão
- [ ] **Leme:** o barco passa a ter direção e inércia (vira aos poucos); leme melhor vira mais rápido
- [ ] **Durabilidade:** bater na costa gasta durabilidade; reparo pago no porto
- [ ] NPC **Estaleiro** no porto com os níveis e preços de cada parte
- [ ] Barco desenhado diferente conforme o nível (ex.: `@`, `Ø`, `▲`)

**Pronta quando:** um jogador com upgrades é claramente mais rápido e aguenta mais que um novato.

## v0.5: PvP, bag, coroa e mercenário

**Objetivo:** disputa por cardumes e áreas.

- [ ] Vida do barco e barra de vida no HUD
- [ ] Tiro com a arma equipada (barra de espaço na direção do barco), com alcance e recarga
- [ ] Dano calculado só no servidor
- [ ] Ao morrer: o jogador dropa uma **bag** (com os peixes do porão) e uma **coroa** no local, e renasce no porto
- [ ] NPC **Mercenário**: entregar a bag de outro jogador devolve os peixes dela para você
- [ ] Zonas seguras ao redor das ilhas com NPC (sem PvP)
- [ ] Bags e coroas aparecem no minimapa

**Pronta quando:** derrotar alguém, pegar a bag e trocar no mercenário funciona do começo ao fim.

## v0.6: Sereia e mecânico

- [ ] NPC **Sereia** numa ilha afastada: roleta da sorte com chance de roubo instantâneo (perde parte do dinheiro) ou missão "derrote o jogador X"
- [ ] Missão aparece no HUD e no minimapa (o alvo é marcado)
- [ ] Levar a **coroa** do alvo para a sereia encanta um item aleatório seu (afixo especial)
- [ ] NPC **Mecânico**: combina dois itens do mesmo tipo (arma + arma, boost + boost, ferramenta + ferramenta) usando o gerador procedural
- [ ] Custo em dinheiro para usar os dois NPCs

**Pronta quando:** dá para fazer o ciclo sereia, missão, coroa e encantamento.

## v0.7: Ecossistema vivo e mapa maior

- [ ] Mapa de 2000 x 2000 dividido em chunks de 64 x 64; o cliente recebe só os chunks próximos
- [ ] Biomas: costa, mar aberto e fossas, com fauna e preços próprios
- [ ] População de peixes por região: pescar demais esvazia, e a região se recupera com o tempo
- [ ] Predadores (tubarões, orcas) que atacam barcos fracos
- [ ] Ciclo de dia e noite (espécies noturnas) e clima (tempestade reduz visão e velocidade)
- [ ] Novas ilhas com NPCs espalhadas pelo mapa

**Pronta quando:** jogadores se espalham pelo mapa em vez de ficar no mesmo lugar.

## v0.8: Bosses

- [ ] Eventos agendados anunciados para todos ("O Kraken surgiu no Norte!")
- [ ] **Kraken**, **Megalodon** e **Cthulhu**, cada um com padrão de ataque próprio e muita vida
- [ ] Dano compartilhado: vários jogadores podem lutar juntos
- [ ] Ao morrer, o boss solta dinheiro no mar para coletar e a arma característica dele
- [ ] Bosses aparecem no minimapa durante o evento

**Pronta quando:** um evento de boss reúne vários jogadores no mesmo lugar.

## v0.9: Servidor online e anti-trapaça

- [ ] Hospedar em Fly.io, Railway ou VPS, com HTTPS e WSS
- [ ] Trocar SQLite por PostgreSQL e ranking em Redis
- [ ] Limite de mensagens por segundo por jogador
- [ ] Validação de todas as ações no servidor (distância, recarga, dinheiro)
- [ ] Temporadas de ranking (ex.: mensais) com prêmios cosméticos

## v1.0: Lançamento

- [ ] Balanceamento de preços, drops e dano com testes de jogadores reais
- [ ] Tutorial curto na primeira entrada
- [ ] Sons simples e efeitos visuais em ASCII (explosões, respingos)
- [ ] Página inicial do jogo e convite para jogar
