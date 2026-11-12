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
| `0x03` | `C_SELL` | `u8` espécie (`0xFF` = vender tudo). Só no cais do porto ou de uma vila |
| `0x04` | `C_EQUIP` | `u8` índice na bolsa (0 a 11). Equipa no slot do tipo do item; se já estiver equipado, desequipa |
| `0x05` | `C_DISCARD` | `u8` índice na bolsa. Joga o item fora (desequipa antes) |
| `0x06` | `C_UPGRADE` | `u8` peça (0 vela, 1 casco, 2 leme). Compra o próximo nível no estaleiro. Só no cais |
| `0x07` | `C_REPAIR` | Conserta o barco inteiro. Só no cais do porto ou de uma vila |
| `0x08` | `C_FIRE` | `u16` x, `u16` y: célula do mundo para onde atirar. Precisa de arma equipada, recarga zerada e estar fora da zona segura |
| `0x09` | `C_MERC` | Entrega o saque ao mercenário: os peixes das bags vão para o porão e as coroas viram recompensa. Só no cais |
| `0x0A` | `C_MERMAID` | `u8` ação: 0 fala (gira a roleta ou entrega a coroa da missão), 1 desiste da missão. Só perto da sereia |
| `0x0B` | `C_COMBINE` | `u8` índice a, `u8` índice b na bolsa: o mecânico junta os dois itens (mesmo tipo) num só, no lugar de `a`. Só no cais |
| `0x0C` | `C_BUY` | `u8` loja (0 armeiro, 1 + v mascate da vila v), `u8` oferta. No armeiro, só no cais do forte; no mascate, só no cais daquela vila |
| `0x0D` | `C_DELIVER` | Entrega a encomenda do mascate da vila onde o barco está |

## Servidor → cliente

| Byte | Mensagem | Quando | Conteúdo |
| --- | --- | --- | --- |
| `0x81` | `S_WELCOME` | ao entrar | `u8` seu id, `u32` semente do mapa, `u32` largura, `u32` altura, `u8` ticks por segundo, `u8` n, n × (`u16` rx, `u16` ry, `u32` checksum) das regiões de teste |
| `0x82` | `S_ERROR` | entrada recusada | `u8` código: 1 nome inválido, 2 nome já online, 3 servidor lotado, 4 versão diferente, 5 senha inválida (tamanho ou caractere), 6 senha errada |
| `0x83` | `S_STATE` | todo tick (10/s) | ver abaixo |
| `0x84` | `S_PLAYER` | quando alguém entra | `u8` id, `u8` cor, `nome` |
| `0x85` | `S_EVENT` | pesca, venda... | `u8` tipo, `u8` espécie, `u16` quantidade, `u32` valor |
| `0x86` | `S_RANK` | a cada 5 s e após vendas | `u8` n, n × (`u32` dinheiro, `u8` online, `nome`) |
| `0x87` | `S_MARKS` | a cada 1 s e quando um saque cai ou some | `u8` n, n × (`u16` x, `u16` y, `u8` tipo, `u8` extra) — pontos do minimapa |
| `0x88` | `S_PRICES` | a cada 1 s e após vendas | `u16` preço em centavos de cada espécie (9) |
| `0x89` | `S_INVENTORY` | ao entrar e quando a bolsa muda | ver abaixo |
| `0x8A` | `S_BOAT` | ao entrar e depois de um upgrade | `u8` nível da vela, `u8` do casco, `u8` do leme (0 a 4) |
| `0x8B` | `S_CATALOG` | a cada 1 s para quem está no cais de uma vila, e logo depois de uma compra, entrega ou troca | ver abaixo |

### `S_STATE`

```
u32 tick
u16 x, u16 y            sua posição
u32 dinheiro
u16 capacidade do porão
u8  velocidade máxima (células/s)
u8  velocidade agora    sobe até a máxima e cai devagar (inércia)
u8  flags               bit 0 = está no cais, bit 1 = barco avariado (vida 0),
                        bit 2 = zona segura, bit 3 = perto da sereia,
                        bit 4 = no cais de uma vila, bit 5 = preso por um tentáculo,
                        bit 6 = louco (leme invertido perto do Cthulhu),
                        bit 7 = no cais do forte do armeiro
u16 vida, u16 vida máxima
u8  recarga             ticks até poder atirar de novo (0 = pronto)
u16 porão[9]            quantidade de cada espécie
u8  jogadores online
u8  n, n × (u8 id, u16 x, u16 y, u8 visual, u8 vida%)   outros barcos a até 50 células;
                        visual = nível do casco × 16 + nível da vela
u8  n, n × (u16 x, u16 y, u8 raio, u8 espécie)   cardumes a até 50 células
u8  n, n × (u16 x, u16 y)                        tiros voando a até 50 células
u8  estoque%            peixe que sobrou na região onde o barco está (0 a 100)
u8  n, n × (u8 tipo, u16 x, u16 y, u8 vida%)     bichos a até 50 células (0 tubarão, 1 navio fantasma)
evento do mundo (sempre vem, 10 bytes):
    u8  tipo            0 nenhum, 1 tempestade, 2 maré de lulas, 3 navio fantasma
    u16 x, u16 y        centro (no navio fantasma, onde ele está)
    u8  raio
    u16 segundos que faltam
    i8  vento dx, i8 vento dy   para onde a tempestade anda (−1, 0 ou 1)
boss (sempre vem, 13 bytes, para todos os jogadores, perto ou longe):
    u8  tipo            0 nenhum, 1 Megalodon, 2 Kraken, 3 Cthulhu
    u16 x, u16 y        centro do corpo
    u8  raio            o tiro acerta a até essa distância do centro
    u16 vida, u16 vida máxima   a vida é uma só para todos
    u16 segundos que faltam     para ele ir embora
    u8  ataque          ticks do ataque que está acontecendo (investida, tentáculos, anel); 0 = nenhum
clima (sempre vem, 3 bytes, igual para todos):
    u8  clima           0 limpo, 1 chuva, 2 neblina
    u16 segundos até o clima mudar
```

Na neblina, a lista de outros barcos e a de bichos só trazem quem está a até 16 células (distância em linha reta). Tiros e o boss continuam vindo.

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
| 14 | você tomou um tiro ou um ataque: `espécie` = id de quem atirou (`0x81` navio fantasma, `0xA0` + tipo do boss), `quantidade` = dano, `valor` = vida que sobrou |
| 15 | não dá para atirar na zona segura |
| 16 | você afundou alguém: `espécie` = id do alvo |
| 17 | você afundou: `espécie` = id de quem atirou (`0xFF` se já saiu; `0x80` tubarão, `0x81` navio fantasma, `0xA1` Megalodon, `0xA2` Kraken, `0xA3` Cthulhu), `quantidade` = peixes que ficaram na bag |
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
| 33 | um bicho te mordeu: `espécie` = tipo do bicho, `quantidade` = dano, `valor` = vida que sobrou |
| 34 | seu tiro acertou um bicho: `espécie` = tipo, `quantidade` = dano, `valor` = vida que sobrou nele |
| 35 | você afundou um bicho: `espécie` = tipo, `valor` = recompensa; no navio fantasma, `quantidade` = seu dano e `valor` = sua parte |
| 36 | começou um evento do mundo: `espécie` = tipo, `quantidade` = segundos, `valor` = x + y × 65536 |
| 37 | o evento do mundo acabou: `espécie` = tipo |
| 38 | apareceu um boss: `espécie` = tipo, `quantidade` = segundos, `valor` = x + y × 65536 |
| 39 | seu tiro acertou o boss: `espécie` = tipo, `quantidade` = dano, `valor` = vida que sobrou nele |
| 40 | o boss foi embora: `espécie` = tipo, `quantidade` = 1 se afundou (0 se o tempo acabou), `valor` = id de quem levou a arma única + 1 (0 = ninguém) |
| 41 | sua parte do prêmio do boss: `espécie` = tipo, `quantidade` = seu dano, `valor` = centavos |
| 42 | o clima mudou (vai para todos): `espécie` = clima novo (0 limpo, 1 chuva, 2 neblina), `quantidade` = segundos que ele dura |
| 43 | comprou no armeiro ou no mascate: `espécie` = tipo do item, `quantidade` = raridade, `valor` = preço |
| 44 | mascate: esse item já foi vendido (volta na próxima troca) |
| 45 | os mascates trocaram o catálogo (vai para todos): `quantidade` = minutos até a próxima troca |
| 46 | entregou a encomenda: `espécie` = espécie, `quantidade` = peixes, `valor` = pagamento (o baú vem num evento 6 ou 7 em seguida) |
| 47 | encomenda: você já entregou neste catálogo |
| 48 | encomenda: falta peixe: `espécie` = espécie, `quantidade` = quantos ele pede, `valor` = quantos você tem |

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

### `S_CATALOG`

```
u8  vila                índice da vila (a mesma ordem das marcas MARK_VILLAGE)
u16 segundos até a troca do catálogo
u8  n                   itens à venda (2)
n × (item (17 bytes, como no S_INVENTORY; tipo 0 = já vendido), u32 preço)
u8  espécie da encomenda
u8  quantos peixes ele pede
u8  já entregou         1 = você já entregou neste catálogo
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
| 11 | Dente do Megalodon | arma única | dano 9–11 |
| 12 | Tentáculo do Kraken | arma única | dano 7–9 |
| 13 | Cajado de R'lyeh | arma única | dano 6–8 |

| Id | Afixo | Stat (comum) |
| --- | --- | --- |
| 0 | Veloz | velocidade 1 |
| 1 | Robusto | vida 5–15 |
| 2 | Afiado | dano 2–5 |
| 3 | do Pescador | produção 4–10 |
| 4 | da Sorte | sorte 3–8 |

As armas únicas só caem dos bosses, são sempre lendárias (×2,2, 3 afixos), não saem de baú e o mecânico não as combina.

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
| Dente do Megalodon | 10 células | 0,8 s | 3 células/tick |
| Tentáculo do Kraken | 12 células | 1,0 s | 3 células/tick |
| Cajado de R'lyeh | 16 células | 1,5 s | 2 células/tick |

Poderes das armas únicas: o **Dente** conserta o seu barco com 25% do dano que causa (no mínimo 1); o **Tentáculo** prende o alvo (barco, bicho ou boss) parado por 2 s; o **Cajado** atira 3 tiros em leque (o do meio e um a uns 14° de cada lado).

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
| 5 vila de pescadores | espécie que ela paga 25% mais caro |
| 6 forte do armeiro | 0 |

Bosses e outros NPCs entram aqui como tipos novos.

### Comércio: armeiro e mascates

- **Forte do armeiro** (na costa de uma ilha, 40 a 90 células do porto, longe da sereia e das vilas, sempre no mesmo lugar para a mesma semente; zona segura de 8 células): vende as armas comuns com a raridade garantida (afixos e números sorteados). Ofertas: 0 Arpão comum $40, 1 Arpão raro $400, 2 Mosquete comum $120, 3 Mosquete raro $1.000, 4 Canhão leve comum $300, 5 Canhão leve raro $2.400. Só cobra se couber na bolsa.
- **Mascate** (no cais de cada vila): 2 itens épicos de qualquer tipo (menos as armas dos bosses), já com encanto +1 ou +2, a $40.000 mais $15.000 por nível de encanto. Uma unidade de cada: quem comprar primeiro leva, e o item fica vendido até a troca.
- **Encomenda** (também no mascate): N peixes de uma espécie que está a 80% ou mais do preço cheio no mercado, uns $300 em peixe (de 5 a 50). Paga o dobro do preço do mercado **na hora da entrega**, mais um baú, e a entrega pesa no mercado como uma venda. Cada perfil entrega uma vez por catálogo.
- O catálogo de todas as vilas troca junto a cada 30 min. Ele não é salvo: o servidor que reinicia sorteia um novo.

### Ecossistema e eventos do mundo

- **Estoque de peixe**: o mapa é dividido em 16 × 16 regiões de 32 × 32 células, cada uma com até 250 peixes. Pescar tira do estoque, e a chance de pescar é multiplicada por `estoque / 250`. O estoque cresce sozinho a cada 5 s (devagar quando está vazio ou cheio, rápido no meio): uma região vazia volta em uns 6 minutos. Cardumes nascem mais onde tem peixe e vão embora quando a região cai abaixo de 20%.
- **Vilas** (3, entre 60 e 200 células do porto, na costa de outras ilhas, sempre no mesmo lugar para a mesma semente; zona segura de 10 células): o lojista e o reparo funcionam, e a vila paga 25% a mais pela espécie preferida. Estaleiro, mercenário e mecânico só no porto.
- **Tubarões** (10 no mar, um novo a cada 30 s se faltar): só nadam em água funda ou normal, fora das zonas seguras. Caçam o barco mais perto a até 15 células que tenha menos de 120 de vida, a 7 células/s, e mordem 8 de vida a cada 1,5 s. Vida 0 por mordida é morte. 40 de vida; afundar um vale $15.
- **Eventos**: um por vez, o primeiro de 2 a 4 min depois de o servidor ligar e depois um a cada 4 a 8 min, sempre a 40 células ou mais do porto e nunca dentro de zona segura.
  - **Tempestade** (2 min, raio 25): anda 1 célula a cada 2 s com o vento; empurra o barco 1 célula por segundo e tira 1 de vida a cada 2 s (só avaria, não afunda); a pesca dentro dela vale o dobro.
  - **Maré de lulas** (1 min 30 s, raio 20, em água funda): 6 cardumes de lula-gigante e o estoque da região cheio.
  - **Navio fantasma** (2 min 30 s): vê qualquer barco a até 20 células, chega a 6 e atira 10 de dano a cada 2 s (alcance 12). 300 de vida; afundá-lo dá $300 divididos pelo dano de cada um e um baú para cada um que acertou. Se o tempo acabar, ele some.

### Bosses

- Um por vez, numa agenda separada dos eventos do mundo (pode ter tempestade e boss juntos): o primeiro de 10 a 20 min depois de o servidor ligar, depois um a cada 20 a 40 min. Sem ninguém online, espera. Nasce a 60 células ou mais do porto, nunca em zona segura, e vai embora depois de 5 min se ninguém afundar.
- Sorteio: Megalodon 45%, Kraken 35%, Cthulhu 20%.
- **Vida compartilhada**: base + um tanto por jogador online na hora em que aparece. O tiro acerta a até `raio` células do centro.
- **Prêmio**: o dinheiro é dividido pelo dano de cada um; a arma única vai para quem mais bateu (com a bolsa cheia, passa para o seguinte); quem deu pelo menos 10% do dano ganha um baú.

| Boss | Vida | Raio | Prêmio | Arma única | Ataque |
| --- | --- | --- | --- | --- | --- |
| Megalodon | 600 + 300 por jogador | 2 | $1500 | Dente | só em água funda; nada a 5 células/s atrás do barco mais perto (até 25); a 12 células dá uma investida de 2 s a 12 células/s e morde 25; descansa 4 s |
| Kraken | 900 + 450 por jogador | 3 | $2500 | Tentáculo | parado; a cada 3 s agarra os 3 barcos mais perto (até 12 células): 12 de dano, puxa 2 células e prende por 2 s |
| Cthulhu | 1500 + 600 por jogador | 4 | $5000 | Cajado | qualquer água, 2 células/s; a cada 5 s um anel de 8 tiros (12 de dano, alcance 16); a até 15 células o leme fica invertido |

### Clima

- Vale para o mapa todo e todo mundo vê igual. O servidor começa com tempo limpo; o primeiro clima vem de 3 a 6 min depois de ligar.
- Chuva ou neblina (meio a meio) duram de 2 a 4 min; depois o tempo fica limpo de 3 a 6 min e sorteia de novo.
- **Chuva**: a pesca rende 25% a mais (soma com a tempestade, que é um evento à parte).
- **Neblina**: só se vê barcos e bichos a até 16 células. O servidor nem manda os de longe.
- **Dia e noite**: o cliente calcula pelo `tick` do `S_STATE` (um dia = 600 s = 6000 ticks), então é a mesma hora para todos. O servidor não usa a hora para nada.

## Senhas

A senha vai pela rede só no `C_JOIN`. O servidor guarda no perfil um sal aleatório de 16 bytes e o hash PBKDF2-HMAC-SHA256 (4096 iterações, 32 bytes) da senha com esse sal; a senha em si não é guardada. No ESP32 o PBKDF2 é o do mbedTLS (API PSA); no servidor do PC é uma implementação em C puro que dá o mesmo resultado (`server/host/test_password.c` confere).

Sem HTTPS a senha viaja aberta no Wi-Fi. Serve para ninguém entrar com o seu nome, não para proteger uma senha que você usa em outro lugar.

## Mapa

O mapa **não** vai pela rede: os dois lados geram o mesmo oceano a partir da semente, usando só inteiros (`world.c` e `world.odin` fazem a mesma conta). O mundo tem 65536 × 65536 tiles (as coordenadas cabem num `u16`), em regiões de 128 × 128 centradas a cada 128 células, com o porto no meio. Ninguém guarda o mundo inteiro: o servidor e o cliente geram em chunks de 64 × 64 só o que fica perto dos barcos. Para conferir a conta, o servidor manda no `S_WELCOME` o checksum FNV-1a de algumas regiões de teste (perto do porto, no mar distante e no abismo). O cliente gera cada uma ao entrar e avisa no diário se alguma ficou diferente.

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
