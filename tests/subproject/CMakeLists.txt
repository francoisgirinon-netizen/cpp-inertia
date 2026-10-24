# Consumes inertia with add_subdirectory (like FetchContent would) and asserts that nothing leaks
# out of a subproject build: no tests, examples, install rules or -Werror.
cmake_minimum_required(VERSION 3.31)
project(inertia_subproject_consumer LANGUAGES CXX)

set(INERTIA_BUILD_DROGON OFF CACHE BOOL "" FORCE) # the core + fake path needs no Drogon
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_subdirectory(${INERTIA_ROOT} inertia)

foreach(_option IN ITEMS INERTIA_BUILD_TESTS INERTIA_BUILD_EXAMPLES INERTIA_INSTALL
                         INERTIA_WARNINGS_AS_ERRORS)
    if(${_option})
        message(FATAL_ERROR "${_option} must default to OFF when inertia is not top-level")
    endif()
endforeach()
if(TARGET inertia_core_tests OR TARGET inertia_example)
    message(FATAL_ERROR "tests or examples were built in a subproject")
endif()

add_executable(consumer ${CMAKE_CURRENT_LIST_DIR}/../package/main_core.cpp)
target_link_libraries(consumer PRIVATE inertia::core inertia::fake)
