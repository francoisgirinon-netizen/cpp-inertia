find_package(Drogon CONFIG REQUIRED)
# Drogon's FindFilesystem leaves CMAKE_CXX_STANDARD defined but empty when it was unset, which
# would make this directory's targets invalid. The standard comes from cxx_std_20 on the core.
if(DEFINED CMAKE_CXX_STANDARD AND CMAKE_CXX_STANDARD STREQUAL "")
    unset(CMAKE_CXX_STANDARD)
endif()

add_library(inertia_drogon STATIC src/convert.cpp src/session.cpp src/http_client.cpp src/install.cpp)
add_library(inertia::drogon ALIAS inertia_drogon)
target_include_directories(inertia_drogon PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(inertia_drogon PUBLIC inertia::core Drogon::Drogon)
target_link_libraries(inertia_drogon PRIVATE inertia_warnings)
