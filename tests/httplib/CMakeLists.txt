add_executable(inertia_httplib_tests)
target_sources(inertia_httplib_tests PRIVATE
    convert_test.cpp)
target_link_libraries(inertia_httplib_tests PRIVATE inertia::httplib Catch2::Catch2WithMain
                                                    inertia_warnings)
catch_discover_tests(inertia_httplib_tests PROPERTIES LABELS httplib)
