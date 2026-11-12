@echo off
rem Compila o servidor de teste e os robôs (bots.exe) no Windows. Rode no "x64 Native Tools Command Prompt for VS".
pushd "%~dp0"
set G=..\components\game
cl /nologo /std:c11 /O2 /W3 /utf-8 /D_CRT_SECURE_NO_WARNINGS /I%G%\include main.c storage_file.c password_host.c %G%\world.c %G%\chunks.c %G%\fish.c %G%\items.c %G%\boat.c %G%\weapon.c %G%\game.c /Fe:cananeia-host.exe ws2_32.lib
cl /nologo /std:c11 /O2 /W3 /utf-8 /D_CRT_SECURE_NO_WARNINGS /I%G%\include bots.c /Fe:bots.exe ws2_32.lib
cl /nologo /std:c11 /O2 /W3 /utf-8 /D_CRT_SECURE_NO_WARNINGS /I%G%\include mapa.c %G%\world.c /Fe:mapa.exe
cl /nologo /std:c11 /O2 /W3 /utf-8 /D_CRT_SECURE_NO_WARNINGS /I%G%\include test_mapa.c %G%\world.c /Fe:test_mapa.exe
popd
