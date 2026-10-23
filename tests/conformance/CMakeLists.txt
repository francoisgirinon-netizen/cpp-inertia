# Adapter-agnostic protocol suite. Each harness supplies send() in its own source file, and
# scenarios.cpp + routes.cpp are shared unchanged.
function(inertia_add_conformance_executable name harness_source)
    set(_dir ${CMAKE_CURRENT_FUNCTION_LIST_DIR})
    add_executable(${name} ${_dir}/scenarios.cpp ${_dir}/routes.cpp ${harness_source})
    target_include_directories(${name} PRIVATE ${_dir})
    target_link_libraries(${name} PRIVATE inertia::core ${ARGN} Catch2::Catch2WithMain
                                          inertia_warnings)
    catch_discover_tests(${name} TEST_PREFIX "${name}: ")
    inertia_add_protocol_tests(${name})
endfunction()

inertia_add_conformance_executable(inertia_conformance_fake
                                   ${CMAKE_CURRENT_SOURCE_DIR}/fake_harness.cpp inertia::fake)
