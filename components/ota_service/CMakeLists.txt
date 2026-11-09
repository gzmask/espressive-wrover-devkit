idf_component_register(SRCS "ota_service.c" "ota_policy.c" INCLUDE_DIRS "include"
    PRIV_REQUIRES app_update esp_https_server esp_timer esp_app_format esp_system
                  esp_partition mbedtls json freertos log wifi_service)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -Wshadow -Wformat=2)
if(CONFIG_OTA_SERVICE_ENABLE)
    if(NOT CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE OR NOT CONFIG_PARTITION_TABLE_CUSTOM)
        message(FATAL_ERROR "OTA requires the approved custom layout and rollback bootloader")
    endif()
    set(OTA_TOKEN_FILE "" CACHE FILEPATH "Private32-byte OTA token")
    set(OTA_CERT_FILE "" CACHE FILEPATH "Private local OTA server certificate")
    set(OTA_KEY_FILE "" CACHE FILEPATH "Private OTA TLS key")
    find_program(BABASHKA bb REQUIRED)
    execute_process(COMMAND "${BABASHKA}" "${PROJECT_DIR}/scripts/validate_ota_files.bb"
        "${OTA_TOKEN_FILE}" "${OTA_CERT_FILE}" "${OTA_KEY_FILE}"
        RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "OTA files failed format/ownership/0600 validation")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                 "${OTA_TOKEN_FILE}" "${OTA_CERT_FILE}" "${OTA_KEY_FILE}")
    target_add_binary_data(${COMPONENT_LIB} "${OTA_TOKEN_FILE}" BINARY RENAME_TO node_ota_token)
    target_add_binary_data(${COMPONENT_LIB} "${OTA_CERT_FILE}" TEXT RENAME_TO node_ota_cert)
    target_add_binary_data(${COMPONENT_LIB} "${OTA_KEY_FILE}" TEXT RENAME_TO node_ota_key)
endif()
