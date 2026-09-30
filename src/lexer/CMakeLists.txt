find_package(FLEX 2.6.4 REQUIRED)
set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${generated_dir}")
FLEX_TARGET(lexer lexer.ll "${generated_dir}/lexer.yy.cpp"
    DEFINES_FILE "${generated_dir}/lexer.yy.h")

add_library(lexer_generated STATIC ${FLEX_lexer_OUTPUTS})
target_compile_features(lexer_generated PRIVATE cxx_std_20)
target_include_directories(lexer_generated 
    PRIVATE "${PUBLIC_HEADER_ROOT}" "${CMAKE_CURRENT_SOURCE_DIR}")

add_module(lexer
    SOURCES lexer.cpp
    DEPS_PUBLIC INT3402E::common
    DEPS_PRIVATE lexer_generated)
target_include_directories(lexer SYSTEM PRIVATE "${generated_dir}")
