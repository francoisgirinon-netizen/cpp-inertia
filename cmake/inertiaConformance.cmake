# inertia_add_conformance_test(<target> <harness source>... [LINK <library>...])
#
# Builds the conformance kit against an adapter: an executable made of the kit's Catch2 scenarios
# (inertia::conformance), your harness sources (which define inertia::conformance::send(), see
# <inertia/conformance/harness.hpp>) and the libraries after LINK. Every scenario becomes a CTest
# test named "<target>: <scenario>", labelled `conformance`.
include_guard(GLOBAL)
include(Catch)

function(inertia_add_conformance_test target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "LINK")
    if(NOT ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "inertia_add_conformance_test(${target}): no harness source given")
    endif()
    add_executable(${target} ${ARG_UNPARSED_ARGUMENTS})
    target_link_libraries(${target} PRIVATE inertia::conformance Catch2::Catch2WithMain
                                            ${ARG_LINK})
    catch_discover_tests(${target} TEST_PREFIX "${target}: " PROPERTIES LABELS conformance)
endfunction()
