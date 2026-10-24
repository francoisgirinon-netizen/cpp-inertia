add_executable(inertia_core_tests)
target_sources(inertia_core_tests PRIVATE
    smoke_test.cpp
    headers_test.cpp
    fake_test.cpp
    html_test.cpp
    page_test.cpp
    consume_test.cpp
    props_test.cpp
    partial_test.cpp
    shared_test.cpp
    defer_test.cpp
    merge_test.cpp
    once_test.cpp
    scroll_test.cpp
    middleware_test.cpp
    errors_test.cpp
    flash_history_test.cpp
    vite_test.cpp
    vite_cache_test.cpp
    ssr_test.cpp)
target_link_libraries(inertia_core_tests PRIVATE inertia::core inertia::fake
                                                 Catch2::Catch2WithMain inertia_warnings)
inertia_enable_iwyu(inertia_core_tests)
catch_discover_tests(inertia_core_tests PROPERTIES LABELS unit)
inertia_add_protocol_tests(inertia_core_tests)
