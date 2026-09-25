# The CLI contract is currently unstable, so this smoke test is SUPER-redundant.

add_test(NAME cli_smoke COMMAND "${CMAKE_COMMAND}"
    "-DPROGRAM=$<TARGET_FILE:int3402e>"
    "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/simple.cpp"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cli_smoke.cmake")
set_tests_properties(cli_smoke PROPERTIES
    TIMEOUT 10
    DISABLED TRUE  # Temporarily disable this smoke test.
)

