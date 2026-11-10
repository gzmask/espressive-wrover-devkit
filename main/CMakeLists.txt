idf_component_register(SRCS "app_main.c" "psram_probe.c" INCLUDE_DIRS "."
                       PRIV_REQUIRES node_status memory_pattern esp_app_format esp_timer esp_system
                                     esp_hw_support esp_psram spi_flash heap freertos log board_display wifi_service ota_service lisp_repl)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -Wformat=2 -Wshadow)
