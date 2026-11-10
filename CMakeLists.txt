cmake_minimum_required(VERSION 3.22)

set(IDF_TARGET esp32 CACHE STRING "Verified classic ESP32 target")
if(NOT IDF_TARGET STREQUAL "esp32")
    message(FATAL_ERROR "This board requires IDF_TARGET=esp32")
endif()
find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "$ENV{IDF_PATH}" rev-parse HEAD
                OUTPUT_VARIABLE IDF_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE IDF_GIT_RESULT)
if(NOT IDF_GIT_RESULT EQUAL 0 OR
   NOT IDF_COMMIT STREQUAL "880a203ae603d08a94d06aa01e0d6761958e5e9e")
    message(FATAL_ERROR "Use pinned ESP-IDF v5.5.6 (880a203ae603d08a94d06aa01e0d6761958e5e9e); see docs/BUILD.md")
endif()

if(NOT DEFINED PROJECT_VER)
    set(PROJECT_VER "0.3.0-single-usb")
endif()
# Default excludes the legacy OTA component. Historical regression must
# explicitly select COMPONENTS="main;ota_service" before dependency expansion.
if(NOT DEFINED COMPONENTS)
    set(COMPONENTS main)
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(wrover_radio_node)
include("${CMAKE_CURRENT_LIST_DIR}/cmake/update_mode.cmake")
