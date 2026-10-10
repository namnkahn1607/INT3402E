add_executable(parser_test parser_test.cpp recovery_test.cpp)
target_link_libraries(parser_test PRIVATE
    INT3402E::parser
    INT3402E::lexer
    int3402e_options
    GTest::gtest_main)
gtest_discover_tests(parser_test PROPERTIES TIMEOUT 10)
