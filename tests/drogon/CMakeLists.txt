# Compiled once and added as objects to every Drogon test executable: the Catch2 listener that
# starts the server lives in it, and a static library would let the linker drop that listener.
add_library(inertia_drogon_test_support OBJECT test_server.cpp)
target_include_directories(inertia_drogon_test_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(inertia_drogon_test_support PUBLIC inertia::drogon Catch2::Catch2
                                                         inertia_warnings)

add_executable(inertia_drogon_tests convert_test.cpp session_test.cpp http_client_test.cpp render_test.cpp csrf_test.cpp concurrent_test.cpp
                                    $<TARGET_OBJECTS:inertia_drogon_test_support>
                                    app.cpp)
target_link_libraries(inertia_drogon_tests PRIVATE inertia_drogon_test_support
                                                   Catch2::Catch2WithMain inertia_warnings)
catch_discover_tests(inertia_drogon_tests)
