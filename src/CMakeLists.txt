add_subdirectory(common)
add_subdirectory(lexer)
add_subdirectory(ast)

add_executable(int3402e main.cpp)
target_link_libraries(int3402e
    PRIVATE INT3402E::lexer INT3402E::ast int3402e_options)
