add_executable(inertia_drogon_tests convert_test.cpp)
target_link_libraries(inertia_drogon_tests PRIVATE inertia::drogon Catch2::Catch2WithMain
                                                   inertia_warnings)
catch_discover_tests(inertia_drogon_tests)
