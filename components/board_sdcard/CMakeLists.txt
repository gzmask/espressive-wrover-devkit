set(board_sdcard_sources "board_sdcard_logic.c" "board_sdcard_diagnostics.c")
if(CONFIG_BOARD_SDCARD_ENABLE)
    list(APPEND board_sdcard_sources "board_sdcard.c" "board_sdcard_file.c" "board_sdcard_source.c")
endif()
# ESP-IDF expands component requirements before sdkconfig is available; keep
# this dependency list unconditional and gate only source compilation above.
idf_component_register(SRCS ${board_sdcard_sources} INCLUDE_DIRS "include"
                       REQUIRES board_display fatfs esp_driver_spi esp_driver_sdspi esp_driver_gpio sdmmc freertos heap log)
# Prevent configuration-sized FIL/large buffers from silently returning to task stacks.
# Stack usage artifacts remain in each private build directory; SDK nested calls
# still require target low-water measurements and are not bounded by this check.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow
                       -Wframe-larger-than=768 -fstack-usage)
