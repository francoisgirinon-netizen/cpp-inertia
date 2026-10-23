find_package(Drogon CONFIG REQUIRED)
# Drogon's FindFilesystem leaves CMAKE_CXX_STANDARD defined but empty when it was unset, which
# would make this directory's targets invalid. The standard comes from cxx_std_20 on the core.
if(DEFINED CMAKE_CXX_STANDARD AND CMAKE_CXX_STANDARD STREQUAL "")
    unset(CMAKE_CXX_STANDARD)
endif()

add_library(inertia_drogon STATIC)
add_library(inertia::drogon ALIAS inertia_drogon)
set_target_properties(inertia_drogon PROPERTIES EXPORT_NAME drogon)
target_sources(inertia_drogon
    PRIVATE
        src/convert.cpp
        src/session.cpp
        src/http_client.cpp
        src/install.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/inertia/drogon/drogon.hpp)
target_link_libraries(inertia_drogon PUBLIC inertia::core Drogon::Drogon)
target_link_libraries(inertia_drogon PRIVATE $<BUILD_LOCAL_INTERFACE:inertia_warnings>)
