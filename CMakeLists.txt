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

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LIBGPIOD_INCLUDE_DIRS}
    ${MOSQUITTO_INCLUDE_DIRS}
    ${SPDLOG_INCLUDE_DIRS}
)

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
