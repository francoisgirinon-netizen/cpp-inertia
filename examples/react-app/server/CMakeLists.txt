# The pages and the routes: framework-agnostic, written against the inertia core only.
add_library(inertia_example_routes STATIC)
target_sources(inertia_example_routes PRIVATE
    app.cpp
    routes.cpp
    home.cpp
    users.cpp
    dashboard.cpp
    feed.cpp
    posts.cpp
    history.cpp
    external.cpp
    fragment.cpp
    props.cpp
    login.cpp
    variants.cpp)
target_link_libraries(inertia_example_routes PUBLIC inertia::core PRIVATE inertia_warnings)
target_include_directories(inertia_example_routes PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# The server reads the Vite build (manifest) and the dev-server hot file from here.
target_compile_definitions(inertia_example_routes PRIVATE
    INERTIA_EXAMPLE_PUBLIC_DIR="${CMAKE_CURRENT_SOURCE_DIR}/public")
inertia_enable_iwyu(inertia_example_routes)

# The thin per-adapter layers.
if(TARGET inertia_drogon)
    add_executable(inertia_example drogon_main.cpp)
    target_link_libraries(inertia_example PRIVATE inertia_example_routes inertia::drogon
                                                  inertia_warnings)
    inertia_enable_iwyu(inertia_example)
endif()

if(TARGET inertia_httplib)
    add_executable(inertia_example_httplib httplib_main.cpp)
    target_link_libraries(inertia_example_httplib PRIVATE inertia_example_routes inertia::httplib
                                                          inertia_warnings)
    inertia_enable_iwyu(inertia_example_httplib)
endif()
