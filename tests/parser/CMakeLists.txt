# Compile the public contract independently of parser method linkage.
add_library(parser_interface_check OBJECT parser_interface_check.cpp)
target_link_libraries(parser_interface_check PRIVATE
    INT3402E::ast
    int3402e_options)

foreach(header DECLARATIONS EXPRESSIONS STATEMENTS ROOT)
    string(TOLOWER "${header}" header_suffix)
    add_library(ast_${header_suffix}_header_check OBJECT parser_interface_check.cpp)
    target_compile_definitions(ast_${header_suffix}_header_check PRIVATE CHECK_AST_${header})
    target_link_libraries(ast_${header_suffix}_header_check PRIVATE
        INT3402E::ast
        int3402e_options)
endforeach()

add_executable(parser_test parser_test.cpp recovery_test.cpp)
target_link_libraries(parser_test PRIVATE
    INT3402E::parser
    INT3402E::lexer
    int3402e_options
    GTest::gtest_main)
gtest_discover_tests(parser_test PROPERTIES TIMEOUT 10)
