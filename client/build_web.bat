@echo off
rem Compila o cliente para a web (WASM) no Windows.
rem Antes: rode "emsdk_env.bat" da pasta do emsdk nesta mesma janela.
setlocal
cd /d "%~dp0"
set OUT=build\web
for /f %%i in ('odin root') do set ODIN_DIR=%%i
if not exist %OUT% mkdir %OUT%

odin build src\main_web -target:js_wasm32 -build-mode:obj -o:speed -define:RAYLIB_WASM_LIB=env.o -vet -strict-style -out:%OUT%\game.wasm.o
if errorlevel 1 exit /b 1

copy /y "%ODIN_DIR%\core\sys\wasm\js\odin.js" %OUT%\ >nul
call emcc -o %OUT%\index.html %OUT%\game.wasm.o "%ODIN_DIR%\vendor\raylib\wasm\libraylib.web.a" -O2 -sUSE_GLFW=3 -sWARN_ON_UNDEFINED_SYMBOLS=0 -sALLOW_MEMORY_GROWTH -sSTACK_SIZE=1MB -lwebsocket.js --shell-file web\index.html
if errorlevel 1 exit /b 1
del %OUT%\game.wasm.o
echo Pronto: %OUT%
