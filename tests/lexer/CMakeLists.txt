add_executable(lexer_test lexer_test.cpp)
target_link_libraries(lexer_test PRIVATE
    INT3402E::lexer
    int3402e_options
    GTest::gtest_main)
gtest_discover_tests(lexer_test PROPERTIES TIMEOUT 10)
