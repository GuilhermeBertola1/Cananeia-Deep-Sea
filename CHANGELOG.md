# Changelog

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
