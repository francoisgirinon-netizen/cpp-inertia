# Install rules, relocatable package config and version file. One export set and one install
# component per library, so a package can be consumed as find_package(inertia COMPONENTS core fake).
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(INERTIA_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/inertia)

set(_inertia_installed core fake)
if(TARGET inertia_drogon)
    list(APPEND _inertia_installed drogon)
endif()
if(TARGET inertia_httplib)
    list(APPEND _inertia_installed httplib)
endif()

# Components: inertia_<name> is the development part (headers, import/static library, the .so
# name link, CMake files); a shared build adds inertia_<name>_runtime with what a program needs at
# run time (the DLL, or the versioned shared object), so that it can be packaged on its own.
set(INERTIA_RUNTIME_COMPONENTS)
foreach(_component IN LISTS _inertia_installed)
    install(TARGETS inertia_${_component}
            EXPORT inertia-${_component}-targets
            RUNTIME COMPONENT inertia_${_component}_runtime
            LIBRARY COMPONENT inertia_${_component}_runtime
                    NAMELINK_COMPONENT inertia_${_component}
            ARCHIVE COMPONENT inertia_${_component}
            FILE_SET HEADERS COMPONENT inertia_${_component})
    if(BUILD_SHARED_LIBS)
        list(APPEND INERTIA_RUNTIME_COMPONENTS inertia_${_component}_runtime)
    endif()
    install(EXPORT inertia-${_component}-targets
            NAMESPACE inertia::
            DESTINATION ${INERTIA_INSTALL_CMAKEDIR}
            COMPONENT inertia_${_component})
endforeach()

# The conformance kit: a header-only INTERFACE target whose Catch2 scenarios are installed as
# sources, to be compiled by the consumer (see conformance/CMakeLists.txt), plus its CMake helper.
if(TARGET inertia_conformance)
    install(TARGETS inertia_conformance
            EXPORT inertia-conformance-targets
            FILE_SET HEADERS COMPONENT inertia_conformance)
    install(EXPORT inertia-conformance-targets
            NAMESPACE inertia::
            DESTINATION ${INERTIA_INSTALL_CMAKEDIR}
            COMPONENT inertia_conformance)
    list(TRANSFORM INERTIA_CONFORMANCE_SOURCES
         PREPEND ${PROJECT_SOURCE_DIR}/conformance/ OUTPUT_VARIABLE _conformance_files)
    install(FILES ${_conformance_files}
            DESTINATION ${INERTIA_CONFORMANCE_DATADIR}
            COMPONENT inertia_conformance)
    install(FILES ${CMAKE_CURRENT_LIST_DIR}/inertiaConformance.cmake
            DESTINATION ${INERTIA_INSTALL_CMAKEDIR}
            COMPONENT inertia_conformance)
endif()

configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/inertiaConfig.cmake.in
    ${PROJECT_BINARY_DIR}/inertiaConfig.cmake
    INSTALL_DESTINATION ${INERTIA_INSTALL_CMAKEDIR})
# 0.x: a minor bump may break the API. Switch to SameMajorVersion at 1.0.
write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/inertiaConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion)
install(FILES ${PROJECT_BINARY_DIR}/inertiaConfig.cmake
              ${PROJECT_BINARY_DIR}/inertiaConfigVersion.cmake
        DESTINATION ${INERTIA_INSTALL_CMAKEDIR}
        COMPONENT inertia_core)

install(FILES ${PROJECT_SOURCE_DIR}/LICENSE ${PROJECT_SOURCE_DIR}/README.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
        COMPONENT inertia_core)
