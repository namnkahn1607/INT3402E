add_executable(tac_reader_test tac_reader_test.cpp)
target_link_libraries(tac_reader_test PRIVATE INT3402E::ir int3402e_options GTest::gtest_main)
gtest_discover_tests(tac_reader_test PROPERTIES TIMEOUT 10)
