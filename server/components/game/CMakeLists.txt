# Núcleo do jogo (igual no PC). storage_nvs.c é a parte que só existe no ESP32.
idf_component_register(SRCS "world.c" "fish.c" "game.c" "storage_nvs.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES nvs_flash)
