# Server runner and cookie-jar client shared by the adapter tests and the conformance harness.
add_library(inertia_httplib_test_support STATIC test_support.cpp)
target_include_directories(inertia_httplib_test_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(inertia_httplib_test_support PUBLIC inertia::httplib inertia_warnings)

add_executable(inertia_httplib_tests)
target_sources(inertia_httplib_tests PRIVATE
    convert_test.cpp
    session_test.cpp
    http_client_test.cpp)
target_link_libraries(inertia_httplib_tests PRIVATE inertia_httplib_test_support
                                                    Catch2::Catch2WithMain inertia_warnings)
catch_discover_tests(inertia_httplib_tests PROPERTIES LABELS httplib)
