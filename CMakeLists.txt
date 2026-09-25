cmake_minimum_required(VERSION 3.28)
project(INT3402E VERSION 0.1.0 LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Use an out-of-source build: cmake --preset debug")
endif()

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(CTest)
option(INT3402E_WARNINGS_AS_ERRORS "Treat project warnings as errors" OFF)

set(PUBLIC_HEADER_ROOT "${PROJECT_SOURCE_DIR}/include")
include(cmake/add-module.cmake)

add_library(int3402e_options INTERFACE)
target_compile_features(int3402e_options INTERFACE cxx_std_20)
target_compile_options(int3402e_options INTERFACE
    "$<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Wshadow;-Wconversion>")
if(INT3402E_WARNINGS_AS_ERRORS)
    target_compile_options(int3402e_options INTERFACE "$<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>")
endif()

if(BUILD_TESTING)
    include(FetchContent)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG 063de7e9578f82b369302001269680b4b1553359  # v1.18.0
    )
    FetchContent_MakeAvailable(googletest)
    include(GoogleTest)
endif()

add_subdirectory(src)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(CMAKE_EXPORT_COMPILE_COMMANDS)
    file(CREATE_LINK "${CMAKE_BINARY_DIR}/compile_commands.json"
                     "${CMAKE_SOURCE_DIR}/compile_commands.json" SYMBOLIC)
endif()
