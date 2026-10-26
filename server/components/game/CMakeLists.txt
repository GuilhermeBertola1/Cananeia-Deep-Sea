# Núcleo do jogo (igual no PC). storage_nvs.c e password_psa.c são as partes que só existem no ESP32.
idf_component_register(SRCS "world.c" "fish.c" "items.c" "boat.c" "game.c" "storage_nvs.c" "password_psa.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES nvs_flash mbedtls)