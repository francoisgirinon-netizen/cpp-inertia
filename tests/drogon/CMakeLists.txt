# Compiled once and added as objects to every Drogon test executable: the Catch2 listener that
# starts the server lives in it, and a static library would let the linker drop that listener.
add_library(inertia_drogon_test_support OBJECT test_server.cpp)
target_include_directories(inertia_drogon_test_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(inertia_drogon_test_support PUBLIC inertia::drogon Catch2::Catch2
                                                         inertia_warnings)

inertia_enable_iwyu(inertia_drogon_test_support)

add_executable(inertia_drogon_tests)
target_sources(inertia_drogon_tests PRIVATE
    convert_test.cpp
    session_test.cpp
    http_client_test.cpp
    render_test.cpp
    csrf_test.cpp
    concurrent_test.cpp
    install_test.cpp
    $<TARGET_OBJECTS:inertia_drogon_test_support>
    app.cpp)
target_link_libraries(inertia_drogon_tests PRIVATE inertia_drogon_test_support
                                                   Catch2::Catch2WithMain inertia_warnings)
inertia_enable_iwyu(inertia_drogon_tests)
catch_discover_tests(inertia_drogon_tests PROPERTIES LABELS drogon)

# Variants that need another application configuration (app() is a process singleton).
add_executable(inertia_drogon_nosession_tests)
target_sources(inertia_drogon_nosession_tests PRIVATE
    nosession_test.cpp
    $<TARGET_OBJECTS:inertia_drogon_test_support>
    nosession_app.cpp)
target_link_libraries(inertia_drogon_nosession_tests PRIVATE inertia_drogon_test_support
                                                             Catch2::Catch2WithMain inertia_warnings)
inertia_enable_iwyu(inertia_drogon_nosession_tests)
catch_discover_tests(inertia_drogon_nosession_tests PROPERTIES LABELS drogon)

add_executable(inertia_drogon_nosession_nocsrf_tests)
target_sources(inertia_drogon_nosession_nocsrf_tests PRIVATE
    nosession_nocsrf_test.cpp
    $<TARGET_OBJECTS:inertia_drogon_test_support>
    nosession_nocsrf_app.cpp)
target_link_libraries(inertia_drogon_nosession_nocsrf_tests PRIVATE inertia_drogon_test_support
                                                                    Catch2::Catch2WithMain
                                                                    inertia_warnings)
inertia_enable_iwyu(inertia_drogon_nosession_nocsrf_tests)
catch_discover_tests(inertia_drogon_nosession_nocsrf_tests PROPERTIES LABELS drogon)

add_executable(inertia_drogon_proxy_tests)
target_sources(inertia_drogon_proxy_tests PRIVATE
    proxy_test.cpp
    $<TARGET_OBJECTS:inertia_drogon_test_support>
    proxy_app.cpp)
target_link_libraries(inertia_drogon_proxy_tests PRIVATE inertia_drogon_test_support
                                                         Catch2::Catch2WithMain inertia_warnings)
inertia_enable_iwyu(inertia_drogon_proxy_tests)
catch_discover_tests(inertia_drogon_proxy_tests PROPERTIES LABELS drogon)
