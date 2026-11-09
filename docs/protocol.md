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
| `0x08` | `C_FIRE` | `u16` x, `u16` y: célula do mundo para onde atirar. Precisa de arma equipada, recarga zerada e estar fora da zona segura |
| `0x09` | `C_MERC` | Entrega o saque ao mercenário: os peixes das bags vão para o porão e as coroas viram recompensa. Só no cais |
| `0x0A` | `C_MERMAID` | `u8` ação: 0 fala (gira a roleta ou entrega a coroa da missão), 1 desiste da missão. Só perto da sereia |
| `0x0B` | `C_COMBINE` | `u8` índice a, `u8` índice b na bolsa: o mecânico junta os dois itens (mesmo tipo) num só, no lugar de `a`. Só no cais |

## Servidor → cliente

| Byte | Mensagem | Quando | Conteúdo |
| --- | --- | --- | --- |
| `0x81` | `S_WELCOME` | ao entrar | `u8` seu id, `u32` semente do mapa, `u16` largura, `u16` altura, `u32` checksum do mapa, `u8` ticks por segundo |
| `0x82` | `S_ERROR` | entrada recusada | `u8` código: 1 nome inválido, 2 nome já online, 3 servidor lotado, 4 versão diferente, 5 senha inválida (tamanho ou caractere), 6 senha errada |
| `0x83` | `S_STATE` | todo tick (10/s) | ver abaixo |
| `0x84` | `S_PLAYER` | quando alguém entra | `u8` id, `u8` cor, `nome` |
| `0x85` | `S_EVENT` | pesca, venda... | `u8` tipo, `u8` espécie, `u16` quantidade, `u32` valor |
| `0x86` | `S_RANK` | a cada 5 s e após vendas | `u8` n, n × (`u32` dinheiro, `u8` online, `nome`) |
| `0x87` | `S_MARKS` | a cada 1 s e quando um saque cai ou some | `u8` n, n × (`u16` x, `u16` y, `u8` tipo, `u8` extra) — pontos do minimapa |
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
u8  flags               bit 0 = está no cais, bit 1 = barco avariado (vida 0),
                        bit 2 = zona segura, bit 3 = perto da sereia
u16 vida, u16 vida máxima
u8  recarga             ticks até poder atirar de novo (0 = pronto)
u16 porão[9]            quantidade de cada espécie
u8  jogadores online
u8  n, n × (u8 id, u16 x, u16 y, u8 visual, u8 vida%)   outros barcos a até 50 células;
                        visual = nível do casco × 16 + nível da vela
u8  n, n × (u16 x, u16 y, u8 raio, u8 espécie)   cardumes a até 50 células
u8  n, n × (u16 x, u16 y)                        tiros voando a até 50 células
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
| 13 | seu tiro acertou: `espécie` = id do alvo, `quantidade` = dano, `valor` = vida que sobrou nele |
| 14 | você tomou um tiro: `espécie` = id de quem atirou, `quantidade` = dano, `valor` = vida que sobrou |
| 15 | não dá para atirar na zona segura |
| 16 | você afundou alguém: `espécie` = id do alvo |
| 17 | você afundou: `espécie` = id de quem atirou (`0xFF` se já saiu), `quantidade` = peixes que ficaram na bag |
| 18 | pegou um saque: `espécie` = tipo (1 bag, 2 coroa), `quantidade` = 1 se era seu |
| 19 | achou um saque, mas o barco já carrega 4 |
| 20 | mercenário: `quantidade` = peixes que foram para o porão, `valor` = recompensa pelas coroas, `espécie` = 1 se sobrou peixe na bag (porão cheio) |
| 21 | mercenário: nada para entregar (`espécie` = 1 se é porque o porão está cheio) |
| 22 | a roleta ainda não girou de novo: `valor` = segundos que faltam |
| 23 | roleta: a sereia roubou `valor` centavos |
| 24 | roleta: missão nova (o nome do alvo vem no `S_INVENTORY`) |
| 25 | a sereia mandou alguém atrás de você: `espécie` = id do caçador |
| 26 | roleta: ela só cantou |
| 27 | missão: você ainda não tem a coroa do alvo |
| 28 | encanto: `espécie` = tipo do item, `quantidade` = nível novo, `valor` = índice na bolsa |
| 29 | missão: nenhum item na bolsa pode ser encantado (bolsa vazia ou tudo no nível 3) |
| 30 | desistiu da missão |
| 31 | mecânico: `espécie` = tipo do item novo, `quantidade` = raridade, `valor` = preço |
| 32 | mecânico: os dois itens precisam ser do mesmo tipo |

### `S_INVENTORY`

```
u8  n                   tamanho da bolsa (12)
n × item (17 bytes):
    u8  tipo            0 = slot vazio
    u8  raridade        0 comum, 1 raro, 2 épico, 3 lendário
    u8  equipado        0 = não; 1 arma, 2 ferramenta, 3 a 5 boosts
    u8  encanto         0 a 3 (já somado nos stats)
    u8  afixos[3]       0xFF = sem afixo
    i16 stats[5]        velocidade, vida, dano, produção, sorte
i16 stats[5]            total do barco (base + equipados)
u16 abates, u16 mortes
u8  n                   saques que o barco carrega (4)
n × (u8 tipo, u16 peixes, nome do dono)          tipo 0 = vazio, 1 bag, 2 coroa
u8  missão              1 = a sereia pediu uma coroa
nome do alvo            vazio sem missão
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

### Armas, PvP e morte

O cliente só diz para onde quer atirar (`C_FIRE`); o servidor cria o tiro, anda com ele uma célula por vez e decide quem acertou. O dano é o stat de dano do item (a raridade e o afixo Afiado contam).

| Arma | Alcance | Recarga | Velocidade do tiro |
| --- | --- | --- | --- |
| Arpão | 8 células | 0,6 s | 2 células/tick |
| Mosquete | 14 células | 1,2 s | 3 células/tick |
| Canhão leve | 18 células | 2,0 s | 2 células/tick |

A tabela está em `server/components/game/weapon.c` e, para a mira, em `client/src/weapon.odin`.

- O tiro acerta um barco na mesma linha, a até 1 célula para o lado. Some ao acertar, ao bater na terra ou no fim do alcance.
- **Zona segura**: a até 21 células do centro do porto ninguém atira e tiro não tira vida.
- **Vida 0 por tiro = morte** (bater na costa continua sendo só avaria; um barco avariado afunda com qualquer tiro). Quem morre deixa no mar uma **bag** com os peixes do porão e uma **coroa**, perde o saque que carregava e renasce perto do porto com o barco inteiro.
- Passar por cima de uma bag ou coroa pega o saque. A sua volta para você (os peixes voltam ao porão); a dos outros fica no barco, até 4. Saque no mar some em 5 minutos.

### NPCs: mercenário, sereia e mecânico

- **Mercenário** (no cais): fica com as bags dos outros e passa os peixes delas para o seu porão, até encher (o resto fica na bag). Paga $25 por coroa, menos pela coroa do alvo da sua missão.
- **Sereia** (numa pedra em mar aberto, 70 a 110 células do porto, sempre no mesmo lugar para a mesma semente; zona segura de 8 células em volta). Sem missão, a roleta gira uma vez por minuto: 40% ela rouba 10% do seu dinheiro (no máximo $100), 45% dá a missão "derrote o jogador X" (alguém online, se tiver), 15% só canta. Com a missão, entregar a coroa do alvo encanta um item aleatório da bolsa.
- **Encanto**: até o nível 3, cada nível soma 25% (no mínimo +1) em cada stat positivo do item. Fica no perfil, ao lado do item, e some se o item for jogado fora.
- **Mecânico** (no cais): junta dois itens do mesmo tipo num só, sorteado de novo pelo gerador. Sai no mínimo com a raridade do melhor dos dois; com raridades iguais, uma acima. O encanto maior fica. Nunca sai comum (dois comuns dão um raro). Preço pela raridade que sai: $80 (raro), $250 (épico), $600 (lendário).

### Marcas do minimapa (`S_MARKS`)

| Tipo | Extra |
| --- | --- |
| 1 cardume | espécie |
| 2 bag | 0 |
| 3 coroa | 0 |
| 4 sereia | 0 |

Bosses e outros NPCs entram aqui como tipos novos.

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
