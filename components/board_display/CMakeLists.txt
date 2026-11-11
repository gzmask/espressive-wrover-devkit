idf_component_register(SRCS "board_display.c" "display_probe_logic.c" "display_pipeline.c" INCLUDE_DIRS "include"
    REQUIRES wifi_service lcd_terminal lisp_app
    PRIV_REQUIRES lisp_perf esp_lcd espressif__esp_lcd_ili9341 esp_driver_spi esp_driver_gpio
                  esp_timer esp_app_format esp_system heap freertos log)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow)
