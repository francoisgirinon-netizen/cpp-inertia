add_library(inertia_fake STATIC src/fake.cpp)
add_library(inertia::fake ALIAS inertia_fake)
target_include_directories(inertia_fake PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(inertia_fake PUBLIC inertia::core)
target_link_libraries(inertia_fake PRIVATE inertia_warnings)
