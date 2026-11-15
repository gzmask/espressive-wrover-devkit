set(clock_sources "lisp_clock.c")
if(CONFIG_LISP_HTTP_ENABLE OR CONFIG_LISP_APP_URL_ENABLE)
    list(APPEND clock_sources "lisp_clock_service.c")
endif()
idf_component_register(SRCS ${clock_sources} INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_timer esp_netif freertos)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow -fstack-usage -Wframe-larger-than=768)
