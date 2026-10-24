# inertia_add_library(<name>
#                     SOURCES <file>...         # private sources
#                     HEADERS <file>...         # public headers, installed (file set HEADERS)
#                     [LINK_PUBLIC <target>...] # usage requirements propagated to consumers)
#
# Creates the STATIC library inertia_<name> with the alias inertia::<name> and exported name
# <name>. Public headers live under include/ of the calling directory. The project warnings are
# build-local: they never reach consumers or the install export.
function(inertia_add_library name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES;HEADERS;LINK_PUBLIC")
    set(target inertia_${name})

    add_library(${target} STATIC)
    add_library(inertia::${name} ALIAS ${target})
    set_target_properties(${target} PROPERTIES
        EXPORT_NAME ${name}
        VERIFY_INTERFACE_HEADER_SETS ON)
    target_sources(${target}
        PRIVATE ${ARG_SOURCES}
        PUBLIC FILE_SET HEADERS BASE_DIRS include FILES ${ARG_HEADERS})
    target_compile_features(${target} PUBLIC cxx_std_20)
    if(ARG_LINK_PUBLIC)
        target_link_libraries(${target} PUBLIC ${ARG_LINK_PUBLIC})
    endif()
    target_link_libraries(${target} PRIVATE $<BUILD_LOCAL_INTERFACE:inertia_warnings>)
endfunction()
