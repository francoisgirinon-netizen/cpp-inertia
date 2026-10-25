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
    disk_file_reader_test.cpp
    ssr_test.cpp)
target_link_libraries(inertia_core_tests PRIVATE inertia::core inertia::fake
                                                 Catch2::Catch2WithMain inertia_warnings)
inertia_enable_iwyu(inertia_core_tests)
catch_discover_tests(inertia_core_tests PROPERTIES LABELS unit)
inertia_add_protocol_tests(inertia_core_tests)

# Allocation-count regression tests. Separate executable: it replaces the global operator new to
# count allocations, which the other tests must not run under. Not on MSVC, which has no
# std::aligned_alloc.
if(NOT MSVC)
    add_executable(inertia_alloc_tests)
    target_sources(inertia_alloc_tests PRIVATE alloc/alloc_test.cpp)
    target_link_libraries(inertia_alloc_tests PRIVATE inertia::core inertia::fake
                                                      Catch2::Catch2WithMain inertia_warnings)
    inertia_enable_iwyu(inertia_alloc_tests)
    catch_discover_tests(inertia_alloc_tests PROPERTIES LABELS unit)
endif()
