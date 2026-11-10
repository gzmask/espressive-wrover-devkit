idf_component_register(SRCS "app_main.c" "psram_probe.c" "firmware_health_logic.c" INCLUDE_DIRS "."
                       PRIV_REQUIRES node_status memory_pattern esp_app_format esp_timer esp_system
                                     esp_hw_support esp_psram spi_flash heap freertos log board_display wifi_service lisp_repl)
# CONFIG choices are unavailable during IDF early requirement expansion.
# The historical component must be explicitly selected by the caller.
if(CONFIG_OTA_SERVICE_ENABLE)
    idf_build_get_property(update_components BUILD_COMPONENTS)
    if(NOT "ota_service" IN_LIST update_components)
        message(FATAL_ERROR "Legacy OTA regression requires COMPONENTS=main;ota_service")
    endif()
    idf_component_optional_requires(PRIVATE ota_service)
endif()
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -Wformat=2 -Wshadow)
