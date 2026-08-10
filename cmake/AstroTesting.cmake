# Registers a small assert-based unit test executable and its ctest entry
# in one call, so individual test CMakeLists don't repeat the same
# add_executable/target_link_libraries/add_test boilerplate.
#
# astro_add_unit_test(<name> SOURCES <src...> LIBS <lib...>)
function(astro_add_unit_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})

    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE ${ARG_LIBS})
    target_include_directories(${name} PRIVATE "${CMAKE_SOURCE_DIR}/tests/unit/testing")

    add_test(NAME ${name} COMMAND ${name})
endfunction()
