add_executable(inertia_example)
target_sources(inertia_example PRIVATE
    main.cpp
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
target_link_libraries(inertia_example PRIVATE inertia::drogon inertia_warnings)
# The server reads the Vite build (manifest) and the dev-server hot file from here.
target_compile_definitions(inertia_example PRIVATE
    INERTIA_EXAMPLE_PUBLIC_DIR="${CMAKE_CURRENT_SOURCE_DIR}/public")
inertia_enable_iwyu(inertia_example)
