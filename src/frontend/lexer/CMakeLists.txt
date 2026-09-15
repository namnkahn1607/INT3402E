find_package(FLEX 2.6 REQUIRED)
set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${generated_dir}")
FLEX_TARGET(lexer lexer.ll "${generated_dir}/lexer.yy.cpp"
    DEFINES_FILE "${generated_dir}/lexer.yy.h"
)

add_library(lexer STATIC ${FLEX_lexer_OUTPUTS})
add_library(INT3402E::lexer ALIAS lexer)
target_compile_features(lexer PUBLIC cxx_std_20)
target_include_directories(lexer PUBLIC "${PROJECT_SOURCE_DIR}/include")
# Flex is currently the scanner API. Keep generated headers separate from
# handwritten headers; SYSTEM avoids third-party header diagnostics.
target_include_directories(lexer SYSTEM PUBLIC "${generated_dir}")
# Do not apply handwritten-code warnings to Flex-generated implementation.
