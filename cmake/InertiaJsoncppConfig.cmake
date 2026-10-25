# inertia_select_jsoncpp_per_config()
#
# Drogon's FindJsoncpp.cmake makes the Jsoncpp_lib target from a single find_library() result. With
# vcpkg on Windows both a Debug and a Release jsoncpp.lib exist, and a multi-config generator (Visual
# Studio) links whichever the search found first into every configuration: a Release build then
# fails with LNK2038 (_ITERATOR_DEBUG_LEVEL, RuntimeLibrary) against the Debug library. Point
# Jsoncpp_lib at the library of the configuration being built. A no-op unless MSVC and vcpkg.
function(inertia_select_jsoncpp_per_config)
    if(NOT MSVC OR NOT TARGET Jsoncpp_lib OR NOT _VCPKG_INSTALLED_DIR OR NOT VCPKG_TARGET_TRIPLET)
        return()
    endif()
    set(_root ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET})
    find_library(INERTIA_JSONCPP_DEBUG NAMES jsoncpp PATHS ${_root}/debug/lib NO_DEFAULT_PATH)
    find_library(INERTIA_JSONCPP_RELEASE NAMES jsoncpp PATHS ${_root}/lib NO_DEFAULT_PATH)
    if(NOT INERTIA_JSONCPP_DEBUG OR NOT INERTIA_JSONCPP_RELEASE)
        message(FATAL_ERROR "jsoncpp: no Debug and Release libraries under ${_root}")
    endif()
    set_property(TARGET Jsoncpp_lib PROPERTY INTERFACE_LINK_LIBRARIES
        "$<IF:$<CONFIG:Debug>,${INERTIA_JSONCPP_DEBUG},${INERTIA_JSONCPP_RELEASE}>")
endfunction()
