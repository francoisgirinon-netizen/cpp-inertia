inertia_add_library(core
    EXPORT_FILE inertia/core_export.hpp
    SOURCES
        disk_file_reader.cpp
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
        ${PROJECT_SOURCE_DIR}/include/inertia/attributes.hpp
        ${PROJECT_SOURCE_DIR}/include/inertia/disk_file_reader.hpp
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

# The other half of the runtime guard in <inertia/attributes.hpp>: MSVC with a shared core only.
if(MSVC AND BUILD_SHARED_LIBS)
    target_sources(inertia_core PRIVATE runtime_guard.cpp)
endif()
