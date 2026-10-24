find_package(Catch2 CONFIG REQUIRED)
include(Catch)
# Discover tests when ctest starts, not by running every test executable at build time (which
# would start the Drogon test server, also under the sanitizers).
set(CMAKE_CATCH_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST)

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

add_test(NAME core_has_no_adapter_frameworks
         COMMAND ${CMAKE_COMMAND} -DINERTIA_SOURCE_DIR=${PROJECT_SOURCE_DIR}
                 -P ${PROJECT_SOURCE_DIR}/cmake/CheckCoreNoFrameworks.cmake)
set_tests_properties(core_has_no_adapter_frameworks PROPERTIES LABELS meta)

# Every public header must compile on its own (the verify targets are not part of `all`).
add_test(NAME headers_self_contained
         COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                 --target all_verify_interface_header_sets)
set_tests_properties(headers_self_contained PROPERTIES LABELS meta)
