#!/bin/sh
# Compila o servidor de teste e os robôs no Linux/macOS: ./build.sh && ./cananeia-host
cd "$(dirname "$0")"
G=../components/game
cc -std=c11 -O2 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -I$G/include \
   main.c storage_file.c password_host.c $G/world.c $G/chunks.c $G/fish.c $G/items.c $G/boat.c $G/weapon.c $G/game.c -o cananeia-host
cc -std=c11 -O2 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -I$G/include bots.c -o bots
cc -std=c11 -O2 -Wall -Wextra -I$G/include mapa.c $G/world.c -o mapa
cc -std=c11 -O2 -Wall -Wextra -I$G/include test_mapa.c $G/world.c -o test_mapa
