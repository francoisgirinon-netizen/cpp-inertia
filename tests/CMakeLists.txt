# The tests themselves are C++20; consumers of the library only need C++17 (see headers_cxx17
# and the package.*17 tests).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Catch2 CONFIG REQUIRED)
include(Catch)
# Discover tests when ctest starts, not by running every test executable at build time (which
# would start the Drogon test server, also under the sanitizers).
set(CMAKE_CATCH_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST)

add_subdirectory(support)
add_subdirectory(core)
if(INERTIA_BUILD_DROGON)
    add_subdirectory(drogon) # before conformance: it defines the Drogon test support
endif()
if(INERTIA_BUILD_HTTPLIB)
    add_subdirectory(httplib) # before conformance: it defines the httplib test support
endif()
add_subdirectory(conformance)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
# Deferred so that every test target registered in later subdirectories is included. It runs in the
# top-level directory scope, where Python3_EXECUTABLE is not visible: expand it now.
cmake_language(EVAL CODE "
    cmake_language(DEFER DIRECTORY [[${PROJECT_SOURCE_DIR}]] CALL inertia_register_coverage_check
                   [[${Python3_EXECUTABLE}]] [[${PROJECT_SOURCE_DIR}]])
")

# The Python tools' own tests (coverage_check, version watch, the generated README table).
add_test(NAME tools_unit_tests
         COMMAND ${Python3_EXECUTABLE} -m unittest discover -s tools/tests
         WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
set_tests_properties(tools_unit_tests PROPERTIES LABELS meta)

if(INERTIA_INSTALL)
    include(InertiaPackageTests)
    inertia_add_package_tests()
endif()

# The warning flags of cmake/Warnings.cmake reach the library's own compile lines. It reads the
# compile database, which the generators other than Makefile and Ninja do not write.
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    string(REPLACE ";" "|" _expected_flags "${INERTIA_EXPECTED_WARNING_FLAGS}")
    add_test(NAME flags_applied
             COMMAND ${CMAKE_COMMAND} -DMODE=project
                     -DCOMPILE_COMMANDS=${CMAKE_BINARY_DIR}/compile_commands.json
                     "-DEXPECTED=${_expected_flags}" -DWERROR=${INERTIA_WARNINGS_AS_ERRORS}
                     -DWERROR_FLAG=${INERTIA_WERROR_FLAG} -DROOT=${PROJECT_SOURCE_DIR}
                     -P ${PROJECT_SOURCE_DIR}/tests/flags/check_effective_flags.cmake)
    set_tests_properties(flags_applied PROPERTIES LABELS meta)
endif()

add_test(NAME core_has_no_adapter_frameworks
         COMMAND ${CMAKE_COMMAND} -DINERTIA_SOURCE_DIR=${PROJECT_SOURCE_DIR}
                 -P ${PROJECT_SOURCE_DIR}/cmake/CheckCoreNoFrameworks.cmake)
set_tests_properties(core_has_no_adapter_frameworks PROPERTIES LABELS meta)

# Every public header must compile on its own (the verify targets are not part of `all`).
add_test(NAME headers_self_contained
         COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                 --target all_verify_interface_header_sets)
set_tests_properties(headers_self_contained PROPERTIES LABELS meta)

# Consumers only need C++17: every installed public header, and the conformance kit's sources that
# consumers compile, must build as strict C++17 (the library's own sources stay C++20). The object
# library is not part of `all`; the test builds it, with the project warnings and -Werror.
file(GLOB_RECURSE _cxx17_headers CONFIGURE_DEPENDS RELATIVE ${PROJECT_SOURCE_DIR}
     ${PROJECT_SOURCE_DIR}/include/*.hpp ${PROJECT_SOURCE_DIR}/adapters/*/include/*.hpp)
set(_cxx17_dir ${CMAKE_CURRENT_BINARY_DIR}/headers_cxx17)
set(_cxx17_units)
foreach(_header IN LISTS _cxx17_headers)
    # An adapter that is not built (core-only preset) has no headers to check either.
    if(_header MATCHES "^adapters/([^/]+)/")
        set(_adapter_target inertia_${CMAKE_MATCH_1})
        if(NOT TARGET ${_adapter_target})
            continue()
        endif()
    endif()
    string(REGEX REPLACE "^(adapters/[^/]+/)?include/" "" _include "${_header}")
    string(REPLACE "/" "_" _unit "${_include}")
    file(CONFIGURE OUTPUT ${_cxx17_dir}/${_unit}.cpp CONTENT "#include <@_include@>\n" @ONLY)
    list(APPEND _cxx17_units ${_cxx17_dir}/${_unit}.cpp)
endforeach()
# Kept out of the compile database: clang-tidy takes the first entry of a file, and the C++17
# units of this test must not be what it judges the kit and the headers with.
add_library(inertia_headers_cxx17 OBJECT EXCLUDE_FROM_ALL ${_cxx17_units})
set_target_properties(inertia_headers_cxx17 PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON
                                                       CXX_EXTENSIONS OFF
                                                       EXPORT_COMPILE_COMMANDS OFF)
target_link_libraries(inertia_headers_cxx17 PRIVATE inertia::core inertia::fake inertia_warnings)
target_compile_options(inertia_headers_cxx17 PRIVATE ${INERTIA_WERROR_FLAG})
foreach(_adapter IN ITEMS drogon httplib conformance)
    if(TARGET inertia::${_adapter})
        target_link_libraries(inertia_headers_cxx17 PRIVATE inertia::${_adapter})
    endif()
endforeach()
add_test(NAME headers_cxx17
         COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target inertia_headers_cxx17)
set_tests_properties(headers_cxx17 PROPERTIES LABELS meta)
