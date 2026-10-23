find_package(Drogon CONFIG REQUIRED)

add_library(inertia_drogon STATIC src/convert.cpp src/session.cpp src/http_client.cpp)
add_library(inertia::drogon ALIAS inertia_drogon)
target_include_directories(inertia_drogon PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(inertia_drogon PUBLIC inertia::core Drogon::Drogon)
target_link_libraries(inertia_drogon PRIVATE inertia_warnings)
