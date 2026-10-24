inertia_add_library(core
    SOURCES
        headers.cpp
        html.cpp
        inertia.cpp
        props.cpp
        props_resolver.cpp
        session.cpp
        ssr.cpp
        vite.cpp
        ${PROJECT_SOURCE_DIR}/include/inertia/detail/props_resolver.hpp # private: only src/ includes it, never installed
    BASE_DIR ${PROJECT_SOURCE_DIR}/include
    HEADERS
        ${PROJECT_SOURCE_DIR}/include/inertia/headers.hpp
        ${PROJECT_SOURCE_DIR}/include/inertia/html.hpp
        ${PROJECT_SOURCE_DIR}/include/inertia/inertia.hpp
        ${PROJECT_SOURCE_DIR}/include/inertia/json.hpp
        ${PROJECT_SOURCE_DIR}/include/inertia/ports/file_reader.hpp
        ${PROJECT_SOURCE_DIR}/include/inertia/ports/http_client.hpp
        ${PROJECT_SOURCE_DIR}/include/inertia/ports/request_view.hpp
        ${PROJECT_SOURCE_DIR}/include/inertia/ports/session_store.hpp
        ${PROJECT_SOURCE_DIR}/include/inertia/props.hpp
        ${PROJECT_SOURCE_DIR}/include/inertia/response.hpp
        ${PROJECT_SOURCE_DIR}/include/inertia/session.hpp
        ${PROJECT_SOURCE_DIR}/include/inertia/ssr.hpp
        ${PROJECT_SOURCE_DIR}/include/inertia/vite.hpp
    LINK_PUBLIC nlohmann_json::nlohmann_json)
