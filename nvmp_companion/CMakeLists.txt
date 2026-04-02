cmake_minimum_required(VERSION 3.20)
project(nvmp_companion VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(nvmp_companion SHARED
    src/main.cpp
    src/nvmp_state.cpp
    src/party_hud.cpp
    src/encounter_toggle.cpp
)

# Include path
target_include_directories(nvmp_companion PRIVATE include)

# NVSE plugin naming
set_target_properties(nvmp_companion PROPERTIES
    OUTPUT_NAME "nvmp_companion"
    SUFFIX ".dll"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Disable some MSVC warnings for xNVSE headers
target_compile_options(nvmp_companion PRIVATE
    /W3 /wd4244 /wd4267 /wd4996 /wd4018 /wd4305
)

# Link against Windows libraries
target_link_libraries(nvmp_companion PRIVATE
    kernel32
    user32
    psapi
)
