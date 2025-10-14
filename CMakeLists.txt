cmake_minimum_required(VERSION 3.10)
project(wiegand_lgpiod)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGPIOD REQUIRED libgpiodcxx libgpiod)

include_directories(${LIBGPIOD_INCLUDE_DIRS})
link_directories(${LIBGPIOD_LIBRARY_DIRS})

add_executable(wiegand_reader src/wiegand_reader.cpp)
target_link_libraries(wiegand_reader ${LIBGPIOD_LIBRARIES})
