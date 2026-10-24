# The conformance kit: the adapter-agnostic protocol scenarios, shipped as the installable
# component `conformance` (inertia::conformance). An adapter author supplies send() and builds the
# kit with inertia_add_conformance_test() (cmake/inertiaConformance.cmake).
#
# The Catch2 TEST_CASEs live in scenarios.cpp, so they must be compiled into the consumer's
# executable. A STATIC library would not do: nothing references a TEST_CASE's registrar, so the
# linker would drop scenarios.o and the suite would silently run zero tests. The sources are
# therefore INTERFACE sources: every consumer target compiles them itself, with its own compiler,
# flags and sanitizers (which also keeps the ABI consistent), and they are installed as data files.
if(INERTIA_BUILD_TESTS)
    find_package(Catch2 CONFIG REQUIRED)
else()
    find_package(Catch2 CONFIG QUIET)
endif()
if(NOT Catch2_FOUND)
    message(STATUS "Catch2 not found: the conformance kit is not built nor installed")
    return()
endif()

include(GNUInstallDirs)
include(inertiaConformance) # inertia_add_conformance_test(), also used by this project's tests
set(INERTIA_CONFORMANCE_DATADIR ${CMAKE_INSTALL_DATADIR}/inertia/conformance)
set(_sources scenarios.cpp routes.cpp test_response.cpp)

add_library(inertia_conformance INTERFACE)
add_library(inertia::conformance ALIAS inertia_conformance)
set_target_properties(inertia_conformance PROPERTIES
    EXPORT_NAME conformance
    VERIFY_INTERFACE_HEADER_SETS ON)
target_sources(inertia_conformance
    INTERFACE FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/include
        FILES ${PROJECT_SOURCE_DIR}/include/inertia/conformance/harness.hpp
              ${PROJECT_SOURCE_DIR}/include/inertia/conformance/routes.hpp)
foreach(_source IN LISTS _sources)
    target_sources(inertia_conformance INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${_source}>
        $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${INERTIA_CONFORMANCE_DATADIR}/${_source}>)
endforeach()
target_compile_features(inertia_conformance INTERFACE cxx_std_20)
# Not WithMain: the consumer picks its main().
target_link_libraries(inertia_conformance INTERFACE inertia::core Catch2::Catch2)

set(INERTIA_CONFORMANCE_SOURCES ${_sources} PARENT_SCOPE)
set(INERTIA_CONFORMANCE_DATADIR ${INERTIA_CONFORMANCE_DATADIR} PARENT_SCOPE)
