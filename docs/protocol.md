# Protocolo de rede

Cliente (navegador, WASM) e servidor (ESP32-S3 ou o servidor de teste do PC) conversam por **WebSocket em `/ws`**, sempre com **frames binários**. Cada frame carrega **uma mensagem**. O primeiro byte é o tipo; os números são **little-endian**.

O servidor é a autoridade: o cliente só manda intenções (para onde quer ir, o que quer vender). Posição, pesca, dinheiro e ranking são decididos no servidor.

Código: `server/components/game/include/protocol.h` (servidor) e `client/src/protocol.odin` (cliente). Mudou um, mude o outro e aumente `PROTOCOL_VERSION`.

## Tipos usados

| Tipo | Bytes | Observação |
| --- | --- | --- |
| `u8` / `i8` | 1 | |
| `u16` / `i16` | 2 | |
| `u32` | 4 | |
| `nome` | 1 + n | `u8` com o tamanho e depois os bytes (até 16, ASCII) |
| `senha` | 1 + n | igual ao `nome`, de 4 a 32 caracteres ASCII visíveis |
| dinheiro | `u32` | sempre em **centavos** |

## Cliente → servidor

| Byte | Mensagem | Conteúdo |
| --- | --- | --- |
| `0x01` | `C_JOIN` | `u8` versão do protocolo, `nome`, `senha`. Nome novo cria o perfil com essa senha |
| `0x02` | `C_INPUT` | `i8` dx, `i8` dy (−1, 0 ou 1). Mandado só quando a direção muda |
| `0x03` | `C_SELL` | `u8` espécie (`0xFF` = vender tudo). Só funciona no cais |
| `0x04` | `C_EQUIP` | `u8` índice na bolsa (0 a 11). Equipa no slot do tipo do item; se já estiver equipado, desequipa |
| `0x05` | `C_DISCARD` | `u8` índice na bolsa. Joga o item fora (desequipa antes) |
| `0x06` | `C_UPGRADE` | `u8` peça (0 vela, 1 casco, 2 leme). Compra o próximo nível no estaleiro. Só no cais |
| `0x07` | `C_REPAIR` | Conserta o barco inteiro no estaleiro. Só no cais |

## Servidor → cliente

| Byte | Mensagem | Quando | Conteúdo |
| --- | --- | --- | --- |
| `0x81` | `S_WELCOME` | ao entrar | `u8` seu id, `u32` semente do mapa, `u16` largura, `u16` altura, `u32` checksum do mapa, `u8` ticks por segundo |
| `0x82` | `S_ERROR` | entrada recusada | `u8` código: 1 nome inválido, 2 nome já online, 3 servidor lotado, 4 versão diferente, 5 senha inválida (tamanho ou caractere), 6 senha errada |
| `0x83` | `S_STATE` | todo tick (10/s) | ver abaixo |
| `0x84` | `S_PLAYER` | quando alguém entra | `u8` id, `u8` cor, `nome` |
| `0x85` | `S_EVENT` | pesca, venda... | `u8` tipo, `u8` espécie, `u16` quantidade, `u32` valor |
| `0x86` | `S_RANK` | a cada 5 s e após vendas | `u8` n, n × (`u32` dinheiro, `u8` online, `nome`) |
| `0x87` | `S_MARKS` | a cada 1 s | `u8` n, n × (`u16` x, `u16` y, `u8` tipo, `u8` extra) — pontos do minimapa |
| `0x88` | `S_PRICES` | a cada 1 s e após vendas | `u16` preço em centavos de cada espécie (9) |
| `0x89` | `S_INVENTORY` | ao entrar e quando a bolsa muda | ver abaixo |
| `0x8A` | `S_BOAT` | ao entrar e depois de um upgrade | `u8` nível da vela, `u8` do casco, `u8` do leme (0 a 4) |

### `S_STATE`

```
u32 tick
u16 x, u16 y            sua posição
u32 dinheiro
u16 capacidade do porão
u8  velocidade máxima (células/s)
u8  velocidade agora    sobe até a máxima e cai devagar (inércia)
u8  flags               bit 0 = está no cais, bit 1 = barco avariado (vida 0)
u16 vida, u16 vida máxima
u16 porão[9]            quantidade de cada espécie
u8  jogadores online
u8  n, n × (u8 id, u16 x, u16 y, u8 visual)      outros barcos a até 50 células;
                        visual = nível do casco × 16 + nível da vela
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
| 6 | achou um baú: `espécie` = tipo do item, `quantidade` = raridade |
| 7 | achou um baú, mas a bolsa está cheia |
| 8 | bateu na costa: `quantidade` = dano, `valor` = vida que sobrou |
| 9 | barco avariado (vida 0): anda a 2 células/s até o reparo |
| 10 | comprou upgrade: `espécie` = peça, `quantidade` = nível novo, `valor` = preço |
| 11 | consertou: `quantidade` = vida recuperada, `valor` = preço |
| 12 | dinheiro não dá: `valor` = preço |

### `S_INVENTORY`

```
u8  n                   tamanho da bolsa (12)
n × item (16 bytes):
    u8  tipo            0 = slot vazio
    u8  raridade        0 comum, 1 raro, 2 épico, 3 lendário
    u8  equipado        0 = não; 1 arma, 2 ferramenta, 3 a 5 boosts
    u8  afixos[3]       0xFF = sem afixo
    i16 stats[5]        velocidade, vida, dano, produção, sorte
i16 stats[5]            total do barco (base + equipados)
```

O item é guardado no servidor só como `u32 semente + u8 tipo`; raridade, afixos e números saem da semente (`server/components/game/items.c`). O servidor manda o item já pronto, então o cliente não roda o gerador.

### Itens

| Id | Item | Slot | Stat principal (comum) |
| --- | --- | --- | --- |
| 1 | Arpão | arma | dano 5–8 |
| 2 | Mosquete | arma | dano 8–12 |
| 3 | Canhão leve | arma | dano 12–18 |
| 4 | Vara de bambu | ferramenta | produção 5–10 |
| 5 | Rede de pesca | ferramenta | produção 12–20 |
| 6 | Rede de arrasto | ferramenta | produção 20–30 |
| 7 | Motor a vapor | boost | velocidade 1–2 |
| 8 | Casco reforçado | boost | vida 15–30 |
| 9 | Amuleto de búzios | boost | sorte 5–12 |
| 10 | Isca encantada | boost | produção 4–8 |

| Id | Afixo | Stat (comum) |
| --- | --- | --- |
| 0 | Veloz | velocidade 1 |
| 1 | Robusto | vida 5–15 |
| 2 | Afiado | dano 2–5 |
| 3 | do Pescador | produção 4–10 |
| 4 | da Sorte | sorte 3–8 |

Raridade: comum 70% (×1,0, 0 ou 1 afixo), raro 22% (×1,3, 1 afixo), épico 7% (×1,65, 2 afixos), lendário 1% (×2,2, 3 afixos). Produção é % a mais na chance de pescar; sorte é a chance de baú, em milésimos, somada aos 4‰ de base a cada peixe.

### Barco e estaleiro

Movimento com inércia: o `C_INPUT` diz para onde o jogador quer ir; a proa gira 45° por vez até apontar para lá, o barco acelera 1 célula/s por tick até a velocidade máxima e, sem tecla, perde 1 célula/s a cada 2 ticks. Parado, vira na hora.

Bater na costa a 4 células/s ou mais tira 2 de vida por célula/s (o cais não estraga). Com vida 0 o barco fica avariado. O reparo custa $0,20 por ponto de vida.

| Nível | Preço | Vela: velocidade | Casco: porão | Casco: vida | Leme: ticks por 45° |
| --- | --- | --- | --- | --- | --- |
| 0 | já vem | +0 | 150 | +0 | 5 |
| 1 | $150 | +1 | 200 | +25 | 4 |
| 2 | $400 | +2 | 260 | +55 | 3 |
| 3 | $1000 | +3 | 330 | +90 | 2 |
| 4 | $2500 | +5 | 420 | +140 | 1 |

A tabela está em `server/components/game/boat.c` e, para a tela do estaleiro, em `client/src/boat.odin`.

### Marcas do minimapa (`S_MARKS`)

| Tipo | Extra |
| --- | --- |
| 1 cardume | espécie |

Bosses, NPCs e missões entram aqui como tipos novos.

## Senhas

A senha vai pela rede só no `C_JOIN`. O servidor guarda no perfil um sal aleatório de 16 bytes e o hash PBKDF2-HMAC-SHA256 (4096 iterações, 32 bytes) da senha com esse sal; a senha em si não é guardada. No ESP32 o PBKDF2 é o do mbedTLS (API PSA); no servidor do PC é uma implementação em C puro que dá o mesmo resultado (`server/host/test_password.c` confere).

Sem HTTPS a senha viaja aberta no Wi-Fi. Serve para ninguém entrar com o seu nome, não para proteger uma senha que você usa em outro lugar.

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
