set(http_sources "lisp_http.c")
if(CONFIG_LISP_HTTP_ENABLE)
    list(APPEND http_sources "lisp_http_service.c")
endif()
idf_component_register(SRCS ${http_sources} INCLUDE_DIRS "include"
    REQUIRES lisp_app_url
    PRIV_REQUIRES lisp_clock esp_http_client mbedtls esp_timer esp_netif wifi_service heap freertos log)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow -fstack-usage -Wframe-larger-than=768)
