cmake_minimum_required(VERSION 3.31...4.4)
# No C++ modules here; without this CMake wants clang-scan-deps, which the toolchain lacks and
# which makes every try_compile (FindThreads, needed by Drogon) fail.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
project(inertia VERSION 0.1.0
        DESCRIPTION "Inertia.js v3 server protocol implementation for C++20"
        LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

option(INERTIA_BUILD_DROGON "Build the Drogon adapter" ON)
option(INERTIA_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})
include(CMakeDependentOption)
cmake_dependent_option(INERTIA_BUILD_EXAMPLES "Build examples (needs the Drogon adapter)" OFF
                       "PROJECT_IS_TOP_LEVEL;INERTIA_BUILD_DROGON" OFF)
option(INERTIA_INSTALL "Generate install rules" ${PROJECT_IS_TOP_LEVEL})
option(INERTIA_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(INERTIA_COVERAGE_STRICT "Fail coverage_check on todo protocol ids" OFF)
set(INERTIA_E2E_LIST "" CACHE FILEPATH
    "Output of `playwright test --list`: coverage_check then also requires e2e tests")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(Warnings)
include(InertiaTargets)
include(ProtocolCoverage)

find_package(nlohmann_json CONFIG REQUIRED)

inertia_add_library(core
    SOURCES
        src/headers.cpp
        src/html.cpp
        src/inertia.cpp
        src/props.cpp
        src/props_resolver.cpp
        src/session.cpp
        src/ssr.cpp
        src/vite.cpp
        include/inertia/detail/props_resolver.hpp # private: only src/ includes it, never installed
    HEADERS
        include/inertia/headers.hpp
        include/inertia/html.hpp
        include/inertia/inertia.hpp
        include/inertia/json.hpp
        include/inertia/ports/file_reader.hpp
        include/inertia/ports/http_client.hpp
        include/inertia/ports/request_view.hpp
        include/inertia/ports/session_store.hpp
        include/inertia/props.hpp
        include/inertia/response.hpp
        include/inertia/session.hpp
        include/inertia/ssr.hpp
        include/inertia/vite.hpp
    LINK_PUBLIC nlohmann_json::nlohmann_json)

add_subdirectory(adapters/fake)
if(INERTIA_BUILD_DROGON)
    add_subdirectory(adapters/drogon)
endif()

if(INERTIA_INSTALL)
    include(InertiaInstall)
endif()

if(INERTIA_BUILD_EXAMPLES)
    add_subdirectory(examples/react-app/server)
endif()

if(INERTIA_BUILD_TESTS)
    enable_testing()
    find_package(Catch2 CONFIG REQUIRED)
    include(Catch)
    # Discover tests when ctest starts, not by running every test executable at build time (which
    # would start the Drogon test server, also under the sanitizers).
    set(CMAKE_CATCH_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST)
    add_executable(inertia_core_tests)
    target_sources(inertia_core_tests PRIVATE
        tests/core/smoke_test.cpp
        tests/core/headers_test.cpp
        tests/core/fake_test.cpp
        tests/core/html_test.cpp
        tests/core/page_test.cpp
        tests/core/props_test.cpp
        tests/core/partial_test.cpp
        tests/core/shared_test.cpp
        tests/core/defer_test.cpp
        tests/core/merge_test.cpp
        tests/core/once_test.cpp
        tests/core/scroll_test.cpp
        tests/core/middleware_test.cpp
        tests/core/errors_test.cpp
        tests/core/flash_history_test.cpp
        tests/core/vite_test.cpp
        tests/core/ssr_test.cpp)
    target_link_libraries(inertia_core_tests PRIVATE inertia::core inertia::fake
                                                     Catch2::Catch2WithMain inertia_warnings)
    catch_discover_tests(inertia_core_tests PROPERTIES LABELS unit)
    inertia_add_protocol_tests(inertia_core_tests)

    if(INERTIA_BUILD_DROGON)
        add_subdirectory(tests/drogon) # before conformance: it defines the Drogon test support
    endif()
    add_subdirectory(tests/conformance)

    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    # Deferred so that every test target registered in later subdirectories is included.
    cmake_language(DEFER DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} CALL inertia_register_coverage_check
                   ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR})

    if(INERTIA_INSTALL)
        include(InertiaPackageTests)
        inertia_add_package_tests()
    endif()

    add_test(NAME core_has_no_drogon
             COMMAND ${CMAKE_COMMAND} -DINERTIA_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckCoreNoDrogon.cmake)
    set_tests_properties(core_has_no_drogon PROPERTIES LABELS meta)

    # Every public header must compile on its own (the verify targets are not part of `all`).
    add_test(NAME headers_self_contained
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                     --target all_verify_interface_header_sets)
    set_tests_properties(headers_self_contained PROPERTIES LABELS meta)
endif()
