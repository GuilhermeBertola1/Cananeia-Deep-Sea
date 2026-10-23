# Cananeia-Deep-Sea

Jogo online multiplayer em arte ASCII: você comanda um barco num oceano cheio de ilhas, pesca enquanto navega e disputa cardumes com outros jogadores.

Esta é a base (Fase 0 do plano): mapa procedural, movimento, vários jogadores ao mesmo tempo, pesca passiva, cardumes, porão, venda no cais e ranking por dinheiro.

## Como rodar

Precisa do [Node.js 22](https://nodejs.org) ou mais novo.

```bash
npm install
npm run dev
```

Abra http://localhost:5173 em duas abas (ou chame um amigo na mesma rede) para ver os jogadores juntos.

Para rodar em produção (um único processo servindo o jogo e o WebSocket na porta 3000):

```bash
npm run build
npm start
```

Variáveis opcionais: `PORT` (padrão 3000) e `SEED` (muda o mapa, padrão 1337).

## Controles

| Tecla | Ação |
| --- | --- |
| WASD ou setas | Navegar (diagonais combinando duas teclas) |
| E | Vender o porão quando estiver no cais (`H`) |

## Legenda do mapa

| Símbolo | O que é |
| --- | --- |
| `@` amarelo | Seu barco |
| `@` colorido | Outros jogadores |
| `~` `-` | Mar (azul escuro = águas profundas, peixes mais caros) |
| `.` | Águas rasas |
| `:` `"` `%` `&` `^` | Areia, grama, floresta, montanha |
| `H` | Cais da ilha-porto |
| `><` ciano | Cardume: fique dentro dele para pescar muito mais |

## Estrutura

```
shared/    código usado pelo servidor e pelo cliente
  map.ts       geração do oceano por seed (ruído fractal)
  fish.ts      espécies, preços e fauna por bioma
  protocol.ts  mensagens do WebSocket
server/    servidor autoritativo (Node + ws)
  index.ts     HTTP + WebSocket + loop de 10 ticks/s
  game.ts      regras: movimento, pesca, cardumes, venda, ranking
client/    navegador (Vite)
  main.ts      teclado, desenho ASCII e HUD
```

O servidor decide tudo; o cliente só envia a direção desejada e desenha o estado que recebe. O mapa não trafega pela rede: os dois lados geram o mesmo oceano a partir da seed.

## Onde mexer para balancear

Em `server/game.ts`: `PASSIVE_CATCH_CHANCE`, `SCHOOL_CATCH_CHANCE`, `MAX_SCHOOLS`, `BASE_CAPACITY` e `BASE_SPEED`. Preços e espécies ficam em `shared/fish.ts`.

## Próximos passos (roadmap)

1. Contas e salvamento (PostgreSQL), NPC lojista de verdade na ilha-porto.
2. Itens procedurais, slots de boost/arma/ferramenta e upgrades de vela, casco, leme e durabilidade.
3. PvP, drop de bag e coroa, mercenário e zonas seguras.
4. Sereia e mecânico.
5. Bosses: Cthulhu, Megalodon e Kraken.
