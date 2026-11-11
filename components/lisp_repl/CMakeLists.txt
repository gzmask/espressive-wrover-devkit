set(repl_sources "lisp_repl.c" "repl_frontend.c" "repl_remote_logic.c" "repl_boot_info.c" "repl_help.c")
if(CONFIG_LISP_REPL_ENABLE)
    list(APPEND repl_sources "lisp_runtime.c" "lisp_app_runtime.c" "lisp_load_runtime.c")
endif()
if(CONFIG_WIFI_REPL_ENABLE)
    list(APPEND repl_sources "repl_tcp.c" "repl_tcp_session.c")
endif()
idf_component_register(SRCS ${repl_sources} INCLUDE_DIRS "include"
    REQUIRES lcd_terminal lisp_app lisp_app_url
    PRIV_REQUIRES lispbm_runtime board_display esp_driver_uart esp_timer esp_app_format heap freertos log wifi_service lwip)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow)
