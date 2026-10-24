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

foreach(_component IN LISTS _inertia_installed)
    install(TARGETS inertia_${_component}
            EXPORT inertia-${_component}-targets
            ARCHIVE COMPONENT inertia_${_component}
            FILE_SET HEADERS COMPONENT inertia_${_component})
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
