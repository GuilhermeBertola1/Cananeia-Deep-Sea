idf_component_register(SRCS "main.c" "wifi.c" "www.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES game esp_http_server esp_wifi esp_eth esp_netif nvs_flash esp_psram)

# O cliente web (client/build/web) vai embutido no firmware: o ESP serve
# a página direto da flash, sem sistema de arquivos.
set(WWW_DIR "${CMAKE_SOURCE_DIR}/../client/build/web")
set(WWW_FILES index.html index.js index.wasm odin.js)
set(HAS_WWW 1)
foreach(f ${WWW_FILES})
    if(NOT EXISTS "${WWW_DIR}/${f}")
        set(HAS_WWW 0)
    endif()
endforeach()

if(HAS_WWW)
    foreach(f ${WWW_FILES})
        target_add_binary_data(${COMPONENT_TARGET} "${WWW_DIR}/${f}" BINARY)
    endforeach()
    target_compile_definitions(${COMPONENT_TARGET} PRIVATE HAS_WWW=1)
else()
    message(WARNING "Cliente web não encontrado em ${WWW_DIR}. Rode client/build_web antes, senão o ESP não terá a página do jogo.")
endif()
