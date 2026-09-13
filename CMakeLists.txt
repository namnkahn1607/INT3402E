cmake_minimum_required(VERSION 3.28)
project(INT3402E)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

# --- Dependencies ---
set(BUILD_GMOCK OFF CACHE BOOL "Disable GMock" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "Disable GTest installation rules" FORCE)
add_subdirectory(external/googletest)

# --- Compile flags ---
add_library(flag_lib INTERFACE)
target_compile_options(flag_lib INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wunreachable-code
)

# --- Public headers ---
set(PUBLIC_HEADER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/include")
add_library(public_lib INTERFACE)
target_include_directories(public_lib INTERFACE ${PUBLIC_HEADER_ROOT})
target_link_libraries(public_lib INTERFACE flag_lib)

# --- Final executable ---
add_executable(int3402e src/main.cpp)
target_link_libraries(int3402e PRIVATE
    flag_lib
    public_lib
)

# --- Testing ---
include(GoogleTest)
