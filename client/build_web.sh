#!/bin/sh
# Compila o cliente para a web (WASM). Precisa do Odin e do emsdk ativado
# (source <emsdk>/emsdk_env.sh). Saída em build/web.
set -e
cd "$(dirname "$0")"
OUT=build/web
ODIN_ROOT=$(odin root)
mkdir -p $OUT

# 1) Odin -> objeto WASM. A raylib vira import "env", resolvido pelo emcc.
odin build src/main_web -target:js_wasm32 -build-mode:obj -o:speed \
    -define:RAYLIB_WASM_LIB=env.o -vet -strict-style -out:$OUT/game.wasm.o

# 2) emcc junta o objeto com a raylib web e gera index.html/js/wasm.
cp "$ODIN_ROOT/core/sys/wasm/js/odin.js" $OUT/
emcc -o $OUT/index.html $OUT/game.wasm.o "$ODIN_ROOT/vendor/raylib/wasm/libraylib.web.a" \
    -O2 -sUSE_GLFW=3 -sWARN_ON_UNDEFINED_SYMBOLS=0 -sALLOW_MEMORY_GROWTH -sSTACK_SIZE=1MB \
    -lwebsocket.js --shell-file web/index.html
rm $OUT/game.wasm.o
echo "Pronto: $OUT"
