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
