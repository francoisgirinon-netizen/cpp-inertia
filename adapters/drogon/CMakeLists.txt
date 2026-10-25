find_package(Drogon CONFIG REQUIRED)
include(InertiaJsoncppConfig)
inertia_select_jsoncpp_per_config()
# Drogon's FindFilesystem leaves CMAKE_CXX_STANDARD defined but empty when it was unset, which
# would make this directory's targets invalid. The standard comes from cxx_std_20 on the core.
if(DEFINED CMAKE_CXX_STANDARD AND CMAKE_CXX_STANDARD STREQUAL "")
    unset(CMAKE_CXX_STANDARD)
endif()

inertia_add_library(drogon
    SOURCES
        src/convert.cpp
        src/session.cpp
        src/http_client.cpp
        src/install.cpp
    HEADERS
        include/inertia/drogon/drogon.hpp
    LINK_PUBLIC inertia::core Drogon::Drogon)
