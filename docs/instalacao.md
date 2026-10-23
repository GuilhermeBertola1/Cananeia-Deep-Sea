# Instalação no Windows

Você vai precisar de quatro coisas: o compilador C da Microsoft, o Odin, o emsdk (para gerar WebAssembly) e o ESP-IDF 6.1 (para o ESP32-S3). Faça uma vez só.

## 1. Visual Studio Build Tools (compilador C da Microsoft)

O Odin usa o linker da Microsoft no Windows, e o servidor de teste do PC é compilado com o `cl`.

1. Baixe o **Build Tools for Visual Studio 2022** em https://visualstudio.microsoft.com/pt-br/downloads/ (seção "Ferramentas para o Visual Studio").
2. No instalador, marque **Desenvolvimento para desktop com C++** e instale.
3. Depois disso aparece no menu Iniciar o atalho **x64 Native Tools Command Prompt for VS 2022**. Use esse terminal para o Odin e para o servidor de teste.

## 2. Odin

1. Baixe o zip do Windows em https://github.com/odin-lang/Odin/releases (arquivo `odin-windows-amd64-dev-AAAA-MM.zip`, versão **dev-2026-09 ou mais nova**).
2. Extraia em `C:\odin`.
3. Adicione `C:\odin` ao PATH: menu Iniciar → "Editar as variáveis de ambiente do sistema" → Variáveis de Ambiente → `Path` → Novo → `C:\odin`.
4. Abra um **x64 Native Tools Command Prompt** novo e teste:

```bat
odin version
```

Para ver o jogo rodando no desktop (modo offline, só navegar):

```bat
cd C:\Users\gasie\OneDrive\Desktop\jogo-barco-ascii
odin run client\src\main_desktop
```

## 3. emsdk (Emscripten)

A raylib na web precisa do Emscripten para virar WebAssembly.

```bat
cd C:\
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
emsdk install latest
emsdk activate latest
```

Em todo terminal novo, antes de compilar o cliente web, rode:

```bat
C:\emsdk\emsdk_env.bat
```

Compile o cliente:

```bat
cd C:\Users\gasie\OneDrive\Desktop\jogo-barco-ascii
client\build_web.bat
```

Sai em `client\build\web` (index.html, index.js, index.wasm, odin.js).

> No PowerShell o `npm.ps1` já tinha dado problema de política de execução; aqui é a mesma coisa com scripts `.ps1`. Usando o **x64 Native Tools Command Prompt** (que é `cmd`) você não esbarra nisso.

## 4. Testar no PC (sem ESP32)

No **x64 Native Tools Command Prompt**:

```bat
cd C:\Users\gasie\OneDrive\Desktop\jogo-barco-ascii
server\host\build.bat
server\host\cananeia-host.exe client\build\web 8080
```

Abra http://localhost:8080 em duas abas, digite um nome em cada uma e navegue. O servidor de teste usa exatamente o mesmo código do jogo que roda no ESP32 (`server\components\game`). Os perfis ficam no arquivo `perfis.bin` da pasta onde você rodou o servidor.

## 5. ESP-IDF 6.1

O jeito oficial no Windows a partir do ESP-IDF 6 é o **ESP-IDF Installation Manager (EIM)**.

1. Instale o EIM: baixe em https://dl.espressif.com/dl/eim/ ou rode `winget install Espressif.EIM`.
2. Abra o **ESP-IDF Installation Manager**.
3. Em *New Installation*, escolha a instalação personalizada (*Custom Installation*) e selecione a versão **v6.1** e o chip **ESP32-S3**. (Pela linha de comando: `eim install -i v6.1`.)
4. Quando terminar, em *Manage Installations* → *Open Dashboard* → selecione a v6.1 → **Open IDF Terminal**. É nesse terminal que o `idf.py` funciona.

Documentação oficial: https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/get-started/windows-setup.html

## 6. Gravar no ESP32-S3

A placa precisa ter **16 MB de flash e 8 MB de PSRAM octal** (muitas vêm escrito N16R8). Conecte no USB e veja a porta COM no Gerenciador de Dispositivos (ex.: `COM5`).

Primeiro compile o cliente web (passo 3), porque ele vai embutido no firmware. Depois, no **IDF Terminal**:

```bat
cd C:\Users\gasie\OneDrive\Desktop\jogo-barco-ascii\server
idf.py set-target esp32s3
idf.py -p COM5 flash monitor
```

No monitor aparece:

```
wifi: Rede "CananeiaDeepSea" criada. Conecte nela e abra http://192.168.4.1
```

Conecte o celular ou o PC na rede `CananeiaDeepSea` (senha `cananeia123`) e abra http://192.168.4.1. Para sair do monitor: `Ctrl+]`.

### Usar a rede Wi-Fi da sua casa

```bat
idf.py menuconfig
```

Entre em **Cananeia Deep Sea** → *Modo do Wi-Fi* → **Entrar numa rede existente**, coloque o nome e a senha da sua rede, salve (S) e grave de novo (`idf.py -p COM5 flash monitor`). O monitor mostra o endereço, tipo `http://192.168.0.42`.

### Testar o firmware sem placa (opcional)

O ESP-IDF tem um emulador (QEMU). Ele não tem Wi-Fi, então o firmware usa uma rede virtual:

```bat
idf.py -B build-qemu -D SDKCONFIG=build-qemu/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.qemu" build
idf.py -B build-qemu qemu --qemu-extra-args="-nic user,model=open_eth,hostfwd=tcp::8080-:80"
```

E abra http://localhost:8080. Fica lento, mas dá para ver o ESP32 de verdade rodando o jogo.

## Problemas comuns

| Problema | O que fazer |
| --- | --- |
| `odin` não é reconhecido | Faltou pôr `C:\odin` no PATH ou abrir um terminal novo |
| Odin reclama de `link.exe` | Use o **x64 Native Tools Command Prompt**, não o cmd comum |
| `emcc` não é reconhecido | Rode `C:\emsdk\emsdk_env.bat` nesse terminal |
| O ESP mostra "Firmware sem o cliente" | Rode `client\build_web.bat` e depois `idf.py build flash` de novo |
| "sem memória para o mapa: a PSRAM está ligada?" | Sua placa não tem PSRAM octal de 8 MB. Se for PSRAM quad, troque em `idf.py menuconfig` → Component config → ESP PSRAM → Mode |
| A porta COM não aparece | Instale o driver USB da placa (CP210x ou CH340) ou use o conector USB nativo do S3 |
