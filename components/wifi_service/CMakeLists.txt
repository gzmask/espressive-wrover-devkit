# IDF expands requirements before sdkconfig; CONFIG_* cannot guard this list.
# Runtime code is still opt-in and unused SDK sections are linker-GC'd.
idf_component_register(SRCS "wifi_service.c" "wifi_scan_logic.c" "wifi_station.c" "wifi_link_logic.c" "wifi_provision.c" INCLUDE_DIRS "include"
                       PRIV_REQUIRES json esp_wifi esp_event esp_netif freertos log esp_timer esp_hw_support)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow -Wformat=2)

if(CONFIG_WIFI_SERVICE_ENABLE AND CONFIG_WIFI_SERVICE_CONNECT_ENABLE)
    set(WIFI_CREDENTIALS_FILE "" CACHE FILEPATH "Private two-line Wi-Fi file embedded in this application")
    if(NOT EXISTS "${WIFI_CREDENTIALS_FILE}" OR IS_DIRECTORY "${WIFI_CREDENTIALS_FILE}")
        message(FATAL_ERROR "Station build requires -DWIFI_CREDENTIALS_FILE=/path/to/owner-only-wifi.txt")
    endif()
    idf_build_get_property(python PYTHON)
    execute_process(COMMAND "${python}" "${PROJECT_DIR}/scripts/provision_wifi.py"
                    --credentials "${WIFI_CREDENTIALS_FILE}" --validate-only
                    RESULT_VARIABLE credentials_result OUTPUT_QUIET ERROR_QUIET)
    if(NOT credentials_result EQUAL 0)
        message(FATAL_ERROR "Wi-Fi credential file failed local format/ownership/permission validation")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${WIFI_CREDENTIALS_FILE}")
    target_add_binary_data(${COMPONENT_LIB} "${WIFI_CREDENTIALS_FILE}" BINARY RENAME_TO node_wifi_credentials)
endif()
