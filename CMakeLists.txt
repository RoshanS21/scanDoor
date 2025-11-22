cmake_minimum_required(VERSION 3.10)
project(door_control_system)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGPIOD REQUIRED libgpiodcxx libgpiod)
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)
find_package(spdlog REQUIRED)
find_package(nlohmann_json 3.10.5 REQUIRED)

# Detect libgpiod version
string(REGEX MATCH "([0-9]+)\\.([0-9]+)" LIBGPIOD_VERSION_MATCH "${LIBGPIOD_VERSION}")
set(LIBGPIOD_MAJOR_VERSION "${CMAKE_MATCH_1}")
set(LIBGPIOD_MINOR_VERSION "${CMAKE_MATCH_2}")

message(STATUS "libgpiod version: ${LIBGPIOD_VERSION}")
message(STATUS "libgpiod major version: ${LIBGPIOD_MAJOR_VERSION}")

# Define GPIOD_V2_API for libgpiod v2.x and later
if(LIBGPIOD_MAJOR_VERSION GREATER_EQUAL 2)
    set(GPIOD_VERSION_FLAG -DGPIOD_V2_API)
    message(STATUS "Using libgpiod v2.x API")
else()
    set(GPIOD_VERSION_FLAG "")
    message(STATUS "Using libgpiod v1.x API")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LIBGPIOD_INCLUDE_DIRS}
    ${MOSQUITTO_INCLUDE_DIRS}
    ${SPDLOG_INCLUDE_DIRS}
)

# Add compile definitions globally for all targets
add_compile_definitions(${GPIOD_VERSION_FLAG})

# Add subdirectories
add_subdirectory(src/core)
add_subdirectory(src/door)
add_subdirectory(src/mqtt)
add_subdirectory(src/utils)

# Main executable
add_executable(door_controller src/main.cpp)
target_link_libraries(door_controller
    door_core
    door_components
    door_mqtt
    door_utils
    ${LIBGPIOD_LIBRARIES}
    ${MOSQUITTO_LIBRARIES}
    spdlog::spdlog
)
