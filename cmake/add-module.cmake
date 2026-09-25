# cmake/add-module.cmake

function(add_module NAME)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPS_PUBLIC;DEPS_PRIVATE" ${ARGN})
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "[add-module] SOURCES is not specified")
    endif()
    if()
        message(FATAL_ERROR "PUBLIC_HEADER_ROOT is not set")
    endif()

    add_library(${NAME} STATIC ${ARG_SOURCES})
    add_library(INT3402E::${NAME} ALIAS ${NAME})

    target_compile_features(${NAME} PUBLIC cxx_std_20)
    target_include_directories(${NAME}
        PUBLIC  $<BUILD_INTERFACE:${PUBLIC_HEADER_ROOT}>
        PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
    target_link_libraries(${NAME} PRIVATE int3402e_options)

    if(ARG_DEPS_PUBLIC)
        target_link_libraries(${NAME} PUBLIC ${ARG_DEPS_PUBLIC})
    endif()
    if(ARG_DEPS_PRIVATE)
        target_link_libraries(${NAME} PRIVATE ${ARG_DEPS_PRIVATE})
    endif()
endfunction()
