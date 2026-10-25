find_package(httplib CONFIG REQUIRED)

inertia_add_library(httplib
    SOURCES
        src/convert.cpp
        src/session.cpp
        src/http_client.cpp
        src/random.cpp
        src/install.cpp
    HEADERS
        include/inertia/httplib/httplib.hpp
    LINK_PUBLIC inertia::core httplib::httplib)

# random.cpp: BCryptGenRandom
if(WIN32)
    target_link_libraries(inertia_httplib PRIVATE bcrypt)
endif()
