cmake_minimum_required(VERSION 3.10)
project(mp4)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -pedantic -Werror)

include_directories(.)
include_directories(include)

find_package(Threads)

add_executable(mp4 mp4.cpp)

add_custom_target(commands_json ALL
    COMMAND cp "compile_commands.json" "${CMAKE_SOURCE_DIR}/"
    DEPENDS ${CMAKE_BINARY_DIR}/compile_commands.json)
