cmake_minimum_required(VERSION 3.10)
project(GasSensorAPI C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Library sources
set(GAS_SENSOR_SOURCES
    gas_sensor.c
)

set(GAS_SENSOR_HEADERS
    gas_sensor.h
)

# Create library
add_library(gas_sensor ${GAS_SENSOR_SOURCES})

# Include directories
target_include_directories(gas_sensor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Platform-specific settings
if(UNIX AND NOT APPLE)
    # Linux
    target_compile_definitions(gas_sensor PRIVATE _GNU_SOURCE)
elseif(APPLE)
    # macOS
    target_compile_definitions(gas_sensor PRIVATE _DARWIN_C_SOURCE)
elseif(WIN32)
    # Windows
    target_link_libraries(gas_sensor PRIVATE ws2_32)
endif()

# Example executable
add_executable(gas_sensor_example gas_sensor_example.c)
target_link_libraries(gas_sensor_example PRIVATE gas_sensor)

# Installation
install(FILES ${GAS_SENSOR_HEADERS} DESTINATION include)
install(TARGETS gas_sensor DESTINATION lib)
install(TARGETS gas_sensor_example DESTINATION bin)
