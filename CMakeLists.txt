cmake_minimum_required(VERSION 3.28)
project(INT3402E VERSION 0.1.0 LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Use an out-of-source build: cmake --preset debug")
endif()

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(CTest)
option(INT3402E_WARNINGS_AS_ERRORS "Treat project warnings as errors" OFF)

add_library(int3402e_options INTERFACE)
target_compile_features(int3402e_options INTERFACE cxx_std_20)
target_compile_options(int3402e_options INTERFACE
    "$<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Wshadow;-Wconversion>")
if(INT3402E_WARNINGS_AS_ERRORS)
    target_compile_options(int3402e_options INTERFACE
        "$<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>")
endif()

add_subdirectory(src)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
