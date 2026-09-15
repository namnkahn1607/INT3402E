include(FetchContent)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG 52eb8108c5bdec04579160ae17225d66034bd723 # v1.17.0
)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

add_executable(lexer_tests frontend/lexer/lexer_test.cpp)
target_link_libraries(lexer_tests PRIVATE INT3402E::lexer int3402e_options GTest::gtest_main)
gtest_discover_tests(lexer_tests PROPERTIES TIMEOUT 10)

add_test(NAME cli_smoke COMMAND "${CMAKE_COMMAND}"
    "-DPROGRAM=$<TARGET_FILE:int3402e>"
    "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/simple.cpp"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cli_smoke.cmake")
set_tests_properties(cli_smoke PROPERTIES TIMEOUT 10)
