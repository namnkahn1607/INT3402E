add_subdirectory(common)
add_subdirectory(frontend)
add_subdirectory(backend)

add_executable(int3402e main.cpp)
target_link_libraries(int3402e PRIVATE INT3402E::lexer int3402e_options)
