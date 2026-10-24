# This project's adapters run the kit that consumers get (inertia_add_conformance_test, from
# cmake/inertiaConformance.cmake); only the warnings and the protocol coverage are in-tree extras.
function(inertia_add_conformance_executable name)
    inertia_add_conformance_test(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE inertia_warnings)
    inertia_add_protocol_tests(${name})
endfunction()

inertia_add_conformance_executable(
    inertia_conformance_fake ${CMAKE_CURRENT_SOURCE_DIR}/fake_harness.cpp LINK inertia::fake)

if(INERTIA_BUILD_DROGON)
    inertia_add_conformance_executable(
        inertia_conformance_drogon
        "${CMAKE_CURRENT_SOURCE_DIR}/drogon_harness.cpp;$<TARGET_OBJECTS:inertia_drogon_test_support>"
        LINK inertia_drogon_test_support)
endif()

if(INERTIA_BUILD_HTTPLIB)
    inertia_add_conformance_executable(
        inertia_conformance_httplib ${CMAKE_CURRENT_SOURCE_DIR}/httplib_harness.cpp
        LINK inertia_httplib_test_support)
endif()
