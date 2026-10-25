include(GenerateExportHeader)

# inertia_add_library(<name>
#                     SOURCES <file>...         # private sources
#                     HEADERS <file>...         # public headers, installed (file set HEADERS)
#                     [BASE_DIR <dir>]          # root of the public headers (default: include)
#                     [EXPORT_FILE <path>]      # generated export header, relative to the include
#                                               # root (default: inertia/<name>/export.hpp)
#                     [LINK_PUBLIC <target>...] # usage requirements propagated to consumers)
#
# Creates the library inertia_<name> with the alias inertia::<name> and exported name <name>. It
# is STATIC, or SHARED when BUILD_SHARED_LIBS is ON (the standard switch). Public headers live
# under BASE_DIR, relative to the calling directory. The project warnings are build-local: they
# never reach consumers or the install export.
#
# Shared libraries: symbols are hidden unless a public header marks them with
# INERTIA_<NAME>_EXPORT (inertia_<name> -> INERTIA_<NAME>_EXPORT), defined by the header that
# generate_export_header() writes to EXPORT_FILE and installs with the other public headers. The
# macro is empty in a static build, which is told so by INERTIA_<NAME>_STATIC_DEFINE. On Linux a
# version script (cmake/inertia.map) also limits the dynamic symbol table to inertia::*, because
# libstdc++ gives the std:: templates that the library instantiates default visibility.
function(inertia_add_library name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "BASE_DIR;EXPORT_FILE" "SOURCES;HEADERS;LINK_PUBLIC")
    if(NOT ARG_BASE_DIR)
        set(ARG_BASE_DIR include)
    endif()
    if(NOT ARG_EXPORT_FILE)
        set(ARG_EXPORT_FILE inertia/${name}/export.hpp)
    endif()
    set(target inertia_${name})
    string(TOUPPER INERTIA_${name} macro_base)

    add_library(${target})
    add_library(inertia::${name} ALIAS ${target})

    # Generated into the build tree, installed through the file set like the other headers.
    set(export_root ${CMAKE_CURRENT_BINARY_DIR}/include)
    # The macro that tells the headers they are being compiled into the library (CMake only sets
    # it for a shared library). The default, <target>_EXPORTS, is easy to clash with.
    set_target_properties(${target} PROPERTIES DEFINE_SYMBOL ${macro_base}_BUILDING)
    generate_export_header(${target}
        BASE_NAME ${macro_base}
        EXPORT_FILE_NAME ${export_root}/${ARG_EXPORT_FILE})

    set_target_properties(${target} PROPERTIES
        EXPORT_NAME ${name}
        VERIFY_INTERFACE_HEADER_SETS ON
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        # 0.x: a minor release may break the API and the ABI, so the soname carries the minor
        # (libinertia_core.so.0.1), like the package config's SameMinorVersion. 1.0 switches it to
        # the major.
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
    if(MSVC)
        # Debug and release libraries can then sit in one directory (and one install prefix).
        set_target_properties(${target} PROPERTIES DEBUG_POSTFIX d)
    endif()
    target_sources(${target}
        PRIVATE ${ARG_SOURCES}
        PUBLIC FILE_SET HEADERS BASE_DIRS ${ARG_BASE_DIR} ${export_root}
               FILES ${ARG_HEADERS} ${export_root}/${ARG_EXPORT_FILE})
    if(NOT BUILD_SHARED_LIBS)
        target_compile_definitions(${target} PUBLIC ${macro_base}_STATIC_DEFINE)
    endif()
    # Consumers only need C++17 (every public header is C++17); building the library needs C++20.
    target_compile_features(${target} PUBLIC cxx_std_17)
    target_compile_features(${target} PRIVATE cxx_std_20)
    if(ARG_LINK_PUBLIC)
        target_link_libraries(${target} PUBLIC ${ARG_LINK_PUBLIC})
    endif()
    target_link_libraries(${target} PRIVATE $<BUILD_LOCAL_INTERFACE:inertia_warnings>)

    if(BUILD_SHARED_LIBS)
        # An installed library finds its siblings (the adapters need the core) next to itself.
        if(APPLE)
            set_target_properties(${target} PROPERTIES INSTALL_RPATH "@loader_path")
        elseif(UNIX)
            set_target_properties(${target} PROPERTIES INSTALL_RPATH "$ORIGIN")
        endif()
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            target_link_options(${target} PRIVATE
                LINKER:--version-script=${PROJECT_SOURCE_DIR}/cmake/inertia.map)
            set_property(TARGET ${target} APPEND PROPERTY
                LINK_DEPENDS ${PROJECT_SOURCE_DIR}/cmake/inertia.map)
        endif()
    endif()
    inertia_enable_iwyu(${target})
endfunction()
