cmake_minimum_required(VERSION 3.25)
project(cpp_inertia VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(INERTIA_BUILD_DROGON "Build the Drogon adapter" ON)
option(INERTIA_BUILD_TESTS "Build tests" ON)
option(INERTIA_BUILD_EXAMPLES "Build examples" OFF)
option(INERTIA_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(INERTIA_COVERAGE_STRICT "Fail coverage_check on todo protocol ids" OFF)

include(cmake/Warnings.cmake)
include(cmake/ProtocolCoverage.cmake)

find_package(nlohmann_json CONFIG REQUIRED)

add_library(inertia_core STATIC)
add_library(inertia::core ALIAS inertia_core)
target_sources(inertia_core PRIVATE src/headers.cpp)
target_include_directories(inertia_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(inertia_core PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(inertia_core PRIVATE inertia_warnings)

add_subdirectory(adapters/fake)

if(INERTIA_BUILD_TESTS)
    enable_testing()
    find_package(Catch2 CONFIG REQUIRED)
    include(Catch)
    add_executable(inertia_core_tests tests/core/smoke_test.cpp tests/core/headers_test.cpp
                                  tests/core/fake_test.cpp)
    target_link_libraries(inertia_core_tests PRIVATE inertia::core inertia::fake
                                                     Catch2::Catch2WithMain inertia_warnings)
    catch_discover_tests(inertia_core_tests)
    inertia_add_protocol_tests(inertia_core_tests)

    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    get_property(_inertia_protocol_targets GLOBAL PROPERTY INERTIA_PROTOCOL_TEST_TARGETS)
    set(_inertia_coverage_args --matrix ${CMAKE_CURRENT_SOURCE_DIR}/docs/protocol-coverage.toml)
    foreach(_inertia_target IN LISTS _inertia_protocol_targets)
        list(APPEND _inertia_coverage_args --tags-from $<TARGET_FILE:${_inertia_target}>)
    endforeach()
    if(INERTIA_COVERAGE_STRICT)
        list(APPEND _inertia_coverage_args --strict)
    endif()
    add_test(NAME coverage_check
             COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tools/coverage_check.py
                     ${_inertia_coverage_args})

    add_test(NAME core_has_no_drogon
             COMMAND ${CMAKE_COMMAND} -DINERTIA_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckCoreNoDrogon.cmake)
endif()
