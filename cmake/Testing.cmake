# cmake/Testing.cmake

# add_unit_test(<target_name> <module_name>
#     SOURCES <test source file>
#     LIBS    <libraries to link against>
# )
function(add_unit_test target_name module_name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    add_executable(${target_name} ${ARG_SOURCES})
    target_include_directories(${target_name}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/${module_name}
    )
    target_link_libraries(${target_name} PRIVATE ${ARG_LIBS} gtest_main gtest)
    gtest_discover_test(${target_name})
endfunction()
