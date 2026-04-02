cmake_minimum_required(VERSION 3.20)
project(nvmp_server_hook VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(nvmp_server_hook SHARED
    src/main.cpp
)

set_target_properties(nvmp_server_hook PROPERTIES
    OUTPUT_NAME "nvmp_server_hook"
    SUFFIX ".dll"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_compile_definitions(nvmp_server_hook PRIVATE
    WIN32_LEAN_AND_MEAN
)

target_compile_options(nvmp_server_hook PRIVATE
    /W3 /wd4996
)

target_link_libraries(nvmp_server_hook PRIVATE
    kernel32
    user32
)

# Injector executable
add_executable(nvmp_inject
    src/injector.cpp
)

set_target_properties(nvmp_inject PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_compile_definitions(nvmp_inject PRIVATE WIN32_LEAN_AND_MEAN)
target_link_libraries(nvmp_inject PRIVATE kernel32 user32)
