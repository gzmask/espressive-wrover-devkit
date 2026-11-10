set(url_sources "lisp_app_url.c")
if(CONFIG_LISP_APP_URL_ENABLE)
    list(APPEND url_sources "lisp_app_url_service.c")
endif()
# Dependencies are resolved before sdkconfig exists. Keep the Kconfig providers
# visible in that pass; disabled profiles never compile/start the service.
idf_component_register(SRCS ${url_sources} INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_http_client mbedtls esp_timer esp_netif wifi_service heap freertos log)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow)
