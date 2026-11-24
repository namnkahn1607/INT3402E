add_executable(common_test diagnostic_test.cpp)
target_link_libraries(common_test PRIVATE
    INT3402E::common
    int3402e_options
    GTest::gtest_main)
gtest_discover_tests(common_test PROPERTIES TIMEOUT 10)
