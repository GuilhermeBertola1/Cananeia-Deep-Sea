#!/bin/sh
# Compila o servidor de teste no Linux/macOS: ./build.sh && ./cananeia-host
cd "$(dirname "$0")"
G=../components/game
cc -std=c11 -O2 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -I$G/include \
   main.c storage_file.c password_host.c $G/world.c $G/fish.c $G/items.c $G/game.c -o cananeia-host
