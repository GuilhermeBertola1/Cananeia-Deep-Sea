# Protocolo de rede

Cliente (navegador, WASM) e servidor (ESP32-S3 ou o servidor de teste do PC) conversam por **WebSocket em `/ws`**, sempre com **frames binários**. Cada frame carrega **uma mensagem**. O primeiro byte é o tipo; os números são **little-endian**.

O servidor é a autoridade: o cliente só manda intenções (para onde quer ir, o que quer vender). Posição, pesca, dinheiro e ranking são decididos no servidor.

Código: `server/components/game/include/protocol.h` (servidor) e `client/src/protocol.odin` (cliente). Mudou um, mude o outro e aumente `PROTOCOL_VERSION`.

## Tipos usados

| Tipo | Bytes | Observação |
| --- | --- | --- |
| `u8` / `i8` | 1 | |
| `u16` | 2 | |
| `u32` | 4 | |
| `nome` | 1 + n | `u8` com o tamanho e depois os bytes (até 16, ASCII) |
| dinheiro | `u32` | sempre em **centavos** |

## Cliente → servidor

| Byte | Mensagem | Conteúdo |
| --- | --- | --- |
| `0x01` | `C_JOIN` | `u8` versão do protocolo, `nome` |
| `0x02` | `C_INPUT` | `i8` dx, `i8` dy (−1, 0 ou 1). Mandado só quando a direção muda |
| `0x03` | `C_SELL` | `u8` espécie (`0xFF` = vender tudo). Só funciona no cais |

## Servidor → cliente

| Byte | Mensagem | Quando | Conteúdo |
| --- | --- | --- | --- |
| `0x81` | `S_WELCOME` | ao entrar | `u8` seu id, `u32` semente do mapa, `u16` largura, `u16` altura, `u32` checksum do mapa, `u8` ticks por segundo |
| `0x82` | `S_ERROR` | entrada recusada | `u8` código: 1 nome inválido, 2 nome já online, 3 servidor lotado, 4 versão diferente |
| `0x83` | `S_STATE` | todo tick (10/s) | ver abaixo |
| `0x84` | `S_PLAYER` | quando alguém entra | `u8` id, `u8` cor, `nome` |
| `0x85` | `S_EVENT` | pesca, venda... | `u8` tipo, `u8` espécie, `u16` quantidade, `u32` valor |
| `0x86` | `S_RANK` | a cada 5 s e após vendas | `u8` n, n × (`u32` dinheiro, `u8` online, `nome`) |
| `0x87` | `S_MARKS` | a cada 1 s | `u8` n, n × (`u16` x, `u16` y, `u8` tipo, `u8` extra) — pontos do minimapa |
| `0x88` | `S_PRICES` | a cada 1 s e após vendas | `u16` preço em centavos de cada espécie (9) |

### `S_STATE`

```
u32 tick
u16 x, u16 y            sua posição
u32 dinheiro
u16 capacidade do porão
u8  velocidade (células/s)
u8  flags               bit 0 = está no cais
u16 porão[9]            quantidade de cada espécie
u8  jogadores online
u8  n, n × (u8 id, u16 x, u16 y)                 outros barcos a até 50 células
u8  n, n × (u16 x, u16 y, u8 raio, u8 espécie)   cardumes a até 50 células
```

### Eventos (`S_EVENT`)

| Tipo | Significado |
| --- | --- |
| 1 | pescou `quantidade` da `espécie` |
| 2 | vendeu `quantidade` peixes por `valor` centavos |
| 3 | porão cheio |
| 4 | nada daquela espécie para vender |
| 5 | boas-vindas |

### Marcas do minimapa (`S_MARKS`)

| Tipo | Extra |
| --- | --- |
| 1 cardume | espécie |

Bosses, NPCs e missões entram aqui como tipos novos.

## Mapa

O mapa **não** vai pela rede: os dois lados geram o mesmo oceano a partir da semente, usando só inteiros (`world.c` e `world.odin` fazem a mesma conta). O servidor manda o checksum FNV-1a dos 512 × 512 tiles no `S_WELCOME` e o cliente avisa no diário se o mapa dele ficou diferente.

## Espécies

| Id | Espécie | Preço base | Bioma |
| --- | --- | --- | --- |
| 0 | Sardinha | $1 | raso |
| 1 | Camarão | $2 | raso |
| 2 | Caranguejo | $4 | raso |
| 3 | Robalo | $4 | água |
| 4 | Atum | $6 | água |
| 5 | Tartaruga | $12 | água |
| 6 | Polvo | $10 | fundo |
| 7 | Peixe-espada | $15 | fundo |
| 8 | Lula-gigante | $40 | fundo |
