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

# --- Lexer auto-generator ---
set(LEXER_DEF "${CMAKE_SOURCE_DIR}/src/lexer/lexer.ll")
set(LEXER_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${LEXER_GEN_DIR})

set(LEXER_GEN_HEADER "${LEXER_GEN_DIR}/lexer.yy.h")
set(LEXER_GEN_SOURCE "${LEXER_GEN_DIR}/lexer.yy.cpp") # Force C++

find_program(FLEX_EXEC flex REQUIRED)
add_custom_command(
    OUTPUT ${LEXER_GEN_SOURCE} ${LEXER_GEN_HEADER}
    COMMAND ${FLEX_EXEC}
    ARGS --header-file=${LEXER_GEN_HEADER} -o ${LEXER_GEN_SOURCE} ${LEXER_DEF}
    DEPENDS ${LEXER_DEF}
)

# --- Compile flags ---
add_library(flag_lib INTERFACE)
target_compile_options(flag_lib INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wunreachable-code
)

# --- Library targets ---
set(PUBLIC_HEADER_ROOT "${CMAKE_SOURCE_DIR}/include")
set(PUBLIC_SOURCE_ROOT "${CMAKE_SOURCE_DIR}/src")

# Support
add_library(support_lib STATIC
    "${PUBLIC_SOURCE_ROOT}/support/diagnostic.cpp"
    "${PUBLIC_SOURCE_ROOT}/support/source_load.cpp"
)
target_include_directories(support_lib PUBLIC "${PUBLIC_HEADER_ROOT}")
target_link_libraries(support_lib PRIVATE flag_lib)

# Generated lexer
add_library(lexer_gen_lib STATIC "${LEXER_GEN_SOURCE}")
target_include_directories(lexer_gen_lib
    PRIVATE "${PUBLIC_HEADER_ROOT}/lexer"
    PUBLIC "${LEXER_GEN_DIR}"
)
target_link_libraries(lexer_gen_lib PRIVATE flag_lib)

# Lexer
# add_library(lexer_lib STATIC "${PUBLIC_SOURCE_ROOT}/lexer/lexer.cpp")
# target_include_directories(lexer_lib PUBLIC "${PUBLIC_HEADER_ROOT}")
# target_link_libraries(lexer_lib PRIVATE flag_lib lexer_gen_lib)

# --- Final executable ---
add_executable(int3402e src/main.cpp)
target_link_libraries(int3402e PRIVATE
    flag_lib
    support_lib
    # lexer_lib
)

# --- Testing ---
include(${CMAKE_SOURCE_DIR}/cmake/Testing.cmake)
include(GoogleTest)

# add_unit_test(<target_name> <module_name>
#     SOURCES <test source file>
#     LIBS    <libraries to link against>
# )
