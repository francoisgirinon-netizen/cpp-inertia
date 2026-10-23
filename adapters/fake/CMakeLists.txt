add_library(inertia_fake STATIC)
add_library(inertia::fake ALIAS inertia_fake)
set_target_properties(inertia_fake PROPERTIES EXPORT_NAME fake)
target_sources(inertia_fake
    PRIVATE
        src/fake.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/inertia/fake/fake.hpp)
target_link_libraries(inertia_fake PUBLIC inertia::core)
target_link_libraries(inertia_fake PRIVATE $<BUILD_LOCAL_INTERFACE:inertia_warnings>)
