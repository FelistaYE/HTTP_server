cmake_minimum_required(VERSION 3.16)
project(high-perf-server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# Release build by default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options("$<$<CONFIG:RELEASE>:-O2>")

add_executable(server
    src/main.cpp
    src/server.cpp
    src/connection.cpp
    src/http_parser.cpp
    src/http_response.cpp
    src/router.cpp
    src/static_server.cpp
    src/logger.cpp
)

target_include_directories(server PRIVATE include)
target_link_libraries(server PRIVATE pthread)
