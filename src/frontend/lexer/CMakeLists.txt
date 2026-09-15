find_package(FLEX 2.6 REQUIRED)
set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${generated_dir}")
FLEX_TARGET(lexer lexer.ll "${generated_dir}/lexer.yy.cpp"
    DEFINES_FILE "${generated_dir}/lexer.yy.h"
)

add_library(lexer_generated STATIC ${FLEX_lexer_OUTPUTS})
target_compile_features(lexer_generated PRIVATE cxx_std_20)
target_include_directories(lexer_generated PRIVATE "${PROJECT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(lexer STATIC lexer.cpp)
add_library(INT3402E::lexer ALIAS lexer)
target_compile_features(lexer PUBLIC cxx_std_20)
target_include_directories(lexer PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_include_directories(lexer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(lexer SYSTEM PRIVATE "${generated_dir}")
target_link_libraries(lexer PUBLIC INT3402E::common PRIVATE lexer_generated int3402e_options)
