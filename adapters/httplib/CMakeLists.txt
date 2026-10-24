find_package(httplib CONFIG REQUIRED)

inertia_add_library(httplib
    SOURCES
        src/convert.cpp
        src/session.cpp
        src/http_client.cpp
    HEADERS
        include/inertia/httplib/httplib.hpp
    LINK_PUBLIC inertia::core httplib::httplib)
