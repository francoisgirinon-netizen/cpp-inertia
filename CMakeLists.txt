cmake_minimum_required(VERSION 3.31...4.4)
# No C++ modules here; without this CMake wants clang-scan-deps, which the toolchain lacks and
# which makes every try_compile (FindThreads, needed by Drogon) fail.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
project(inertia VERSION 0.1.0
        DESCRIPTION "Inertia.js v3 server protocol implementation for C++"
        LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

option(INERTIA_BUILD_DROGON "Build the Drogon adapter" ON)
option(INERTIA_BUILD_HTTPLIB "Build the cpp-httplib adapter" ON)
option(INERTIA_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})
include(CMakeDependentOption)
if(INERTIA_BUILD_DROGON OR INERTIA_BUILD_HTTPLIB)
    set(_inertia_has_adapter ON)
else()
    set(_inertia_has_adapter OFF)
endif()
cmake_dependent_option(INERTIA_BUILD_EXAMPLES "Build examples (needs the Drogon or the cpp-httplib adapter)" OFF
                       "PROJECT_IS_TOP_LEVEL;_inertia_has_adapter" OFF)
option(INERTIA_BUILD_BENCHMARKS "Build the benchmarks (use a Release build; not run by ctest)" OFF)
option(INERTIA_INSTALL "Generate install rules" ${PROJECT_IS_TOP_LEVEL})
option(INERTIA_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(INERTIA_IWYU "Run include-what-you-use on the project's own targets and fail on its suggestions" OFF)
option(INERTIA_COVERAGE_STRICT "Fail coverage_check on todo protocol ids" OFF)
option(INERTIA_BUILD_DOCS "Build the Doxygen API reference (target docs, not part of all)" OFF)
set(INERTIA_E2E_LIST "" CACHE FILEPATH
    "Output of `playwright test --list`: coverage_check then also requires e2e tests")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(Warnings)
include(InertiaIwyu)
include(InertiaTargets)
include(ProtocolCoverage)
if(INERTIA_BUILD_DOCS)
    include(InertiaDocs)
endif()

find_package(nlohmann_json CONFIG REQUIRED)

add_subdirectory(src)

add_subdirectory(adapters/fake)
if(INERTIA_BUILD_DROGON)
    add_subdirectory(adapters/drogon)
endif()
if(INERTIA_BUILD_HTTPLIB)
    add_subdirectory(adapters/httplib)
endif()

# The conformance kit is a library others build their adapter tests with: built for the tests of
# this project and for the install, so not only with the tests.
if(INERTIA_BUILD_TESTS OR INERTIA_INSTALL)
    add_subdirectory(conformance)
endif()

if(INERTIA_INSTALL)
    include(InertiaInstall)
    include(InertiaPackaging)
endif()

if(INERTIA_BUILD_EXAMPLES)
    add_subdirectory(examples/react-app/server)
endif()

if(INERTIA_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(INERTIA_BUILD_TESTS)
    enable_testing() # at the top level so that ctest finds the tests of every directory
    add_subdirectory(tests)
endif()

if(PROJECT_IS_TOP_LEVEL)
    include(FeatureSummary)
    add_feature_info(drogon INERTIA_BUILD_DROGON "Drogon adapter (inertia::drogon)")
    add_feature_info(httplib INERTIA_BUILD_HTTPLIB "cpp-httplib adapter (inertia::httplib)")
    add_feature_info(tests INERTIA_BUILD_TESTS "Catch2 tests, conformance suites, coverage_check")
    add_feature_info(benchmarks INERTIA_BUILD_BENCHMARKS "benchmarks (target inertia_benchmarks)")
    add_feature_info(examples INERTIA_BUILD_EXAMPLES "React example application")
    add_feature_info(install INERTIA_INSTALL "install rules and package config")
    add_feature_info(docs INERTIA_BUILD_DOCS "Doxygen API reference (target docs)")
    add_feature_info(iwyu INERTIA_IWYU "include-what-you-use on the project's own targets")
    add_feature_info(warnings-as-errors INERTIA_WARNINGS_AS_ERRORS "-Werror (/WX on MSVC) on the project's own targets")
    feature_summary(WHAT ENABLED_FEATURES DISABLED_FEATURES)
endif()
