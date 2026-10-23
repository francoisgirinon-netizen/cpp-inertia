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

find_package(nlohmann_json CONFIG REQUIRED)

# Placeholder translation unit so the static library has something to archive;
# replaced by real sources in later tasks.
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/inertia_core_stub.cpp
     "namespace inertia { void coreStub(); void coreStub() {} }\n")
add_library(inertia_core STATIC ${CMAKE_CURRENT_BINARY_DIR}/inertia_core_stub.cpp)
add_library(inertia::core ALIAS inertia_core)
target_include_directories(inertia_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(inertia_core PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(inertia_core PRIVATE inertia_warnings)

if(INERTIA_BUILD_TESTS)
    enable_testing()
    find_package(Catch2 CONFIG REQUIRED)
    include(Catch)
    add_executable(inertia_core_tests tests/core/smoke_test.cpp)
    target_link_libraries(inertia_core_tests PRIVATE inertia::core Catch2::Catch2WithMain
                                                     inertia_warnings)
    catch_discover_tests(inertia_core_tests)
endif()
