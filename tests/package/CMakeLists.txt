# Out-of-tree consumer of the *installed* inertia package. Driven by the package.* CTest fixtures
# (cmake/InertiaPackageTests.cmake); never added to the main build.
cmake_minimum_required(VERSION 3.31)
project(inertia_consumer LANGUAGES CXX)

set(CONSUMER_COMPONENTS "core,fake" CACHE STRING "comma separated inertia components to request")
string(REPLACE "," ";" CONSUMER_COMPONENTS "${CONSUMER_COMPONENTS}")
find_package(inertia CONFIG REQUIRED COMPONENTS ${CONSUMER_COMPONENTS})

# Project warnings and -Werror must never leak into consumers.
foreach(_target IN LISTS CONSUMER_COMPONENTS)
    get_target_property(_options inertia::${_target} INTERFACE_COMPILE_OPTIONS)
    get_target_property(_libs inertia::${_target} INTERFACE_LINK_LIBRARIES)
    if(_options OR _libs MATCHES "inertia_warnings")
        message(FATAL_ERROR "inertia::${_target} leaks build-local flags: ${_options} / ${_libs}")
    endif()
endforeach()

if("drogon" IN_LIST CONSUMER_COMPONENTS)
    add_executable(consumer main_drogon.cpp)
    target_link_libraries(consumer PRIVATE inertia::drogon)
else()
    add_executable(consumer main_core.cpp)
    target_link_libraries(consumer PRIVATE inertia::core inertia::fake)
endif()
