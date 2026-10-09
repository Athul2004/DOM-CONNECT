cmake_minimum_required(VERSION 3.16)
project(zigbee-gateway C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

add_compile_definitions(_DEFAULT_SOURCE _POSIX_C_SOURCE=200809L)

include_directories(include)
include_directories(vendor/cjson)

# Source files
set(SOURCES
    src/core/error.c
    src/core/log.c
    src/core/config.c
    src/transport/uart_transport.c
    src/transport/mock_transport.c
    src/ezsp/ash.c
    src/ezsp/ezsp_protocol.c
    src/ezsp/ezsp_handler.c
    src/adapter/ezsp_adapter.c
    src/adapter/znp_adapter.c
    vendor/cjson/cJSON.c
    src/mqtt_handler.c
    src/zcl.c
    src/database.c
    src/converters.c
    src/main.c
)

add_executable(zigbee-gateway ${SOURCES})
target_link_libraries(zigbee-gateway mosquitto)

option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
