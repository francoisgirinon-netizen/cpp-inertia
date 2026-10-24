# CPack packaging of the install rules (cmake/InertiaInstall.cmake): one CPack component per
# install component, a TGZ with everything in one archive and one .deb per component.
#
#   cmake --workflow --preset package
#
# Reproducibility: CPack's archive generators and dpkg-deb both honour SOURCE_DATE_EPOCH, so
# exporting it makes the file timestamps (and so the checksums) of the packages stable.
set(CPACK_PACKAGE_NAME inertia)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VENDOR "cpp-inertia maintainers")
set(CPACK_PACKAGE_CONTACT "cpp-inertia maintainers")
set(CPACK_RESOURCE_FILE_LICENSE ${PROJECT_SOURCE_DIR}/LICENSE)
set(CPACK_RESOURCE_FILE_README ${PROJECT_SOURCE_DIR}/README.md)

# <name>-<version>-<system>-<arch>: also the name of the top directory inside the archive.
set(INERTIA_PACKAGE_FILE_NAME
    ${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR})
set(CPACK_PACKAGE_FILE_NAME ${INERTIA_PACKAGE_FILE_NAME})

# The CPack components are the install components of InertiaInstall.cmake.
set(CPACK_COMPONENTS_ALL inertia_core inertia_fake)
set(CPACK_COMPONENT_INERTIA_CORE_DISPLAY_NAME "Core")
set(CPACK_COMPONENT_INERTIA_CORE_DESCRIPTION
    "The Inertia.js v3 protocol implementation: headers, static library, CMake package files, license")
set(CPACK_COMPONENT_INERTIA_FAKE_DISPLAY_NAME "Fake adapter")
set(CPACK_COMPONENT_INERTIA_FAKE_DESCRIPTION
    "In-memory adapter for testing applications without a web framework")
set(CPACK_COMPONENT_INERTIA_FAKE_DEPENDS inertia_core)
if(TARGET inertia_drogon)
    list(APPEND CPACK_COMPONENTS_ALL inertia_drogon)
    set(CPACK_COMPONENT_INERTIA_DROGON_DISPLAY_NAME "Drogon adapter")
    set(CPACK_COMPONENT_INERTIA_DROGON_DESCRIPTION "Adapter for the Drogon web framework")
    set(CPACK_COMPONENT_INERTIA_DROGON_DEPENDS inertia_core)
endif()
if(TARGET inertia_httplib)
    list(APPEND CPACK_COMPONENTS_ALL inertia_httplib)
    set(CPACK_COMPONENT_INERTIA_HTTPLIB_DISPLAY_NAME "cpp-httplib adapter")
    set(CPACK_COMPONENT_INERTIA_HTTPLIB_DESCRIPTION "Adapter for the cpp-httplib web framework")
    set(CPACK_COMPONENT_INERTIA_HTTPLIB_DEPENDS inertia_core)
endif()
if(TARGET inertia_conformance)
    list(APPEND CPACK_COMPONENTS_ALL inertia_conformance)
    set(CPACK_COMPONENT_INERTIA_CONFORMANCE_DISPLAY_NAME "Conformance kit")
    set(CPACK_COMPONENT_INERTIA_CONFORMANCE_DESCRIPTION
        "Catch2 scenarios to check a custom adapter against the protocol")
    set(CPACK_COMPONENT_INERTIA_CONFORMANCE_DEPENDS inertia_core)
endif()

# TGZ: every component in a single archive.
set(CPACK_ARCHIVE_COMPONENT_INSTALL OFF)
set(CPACK_GENERATOR TGZ)

# DEB: one package per component. The libraries are static, so no shlibdeps. Drogon, cpp-httplib
# and Catch2 have no stable Debian package names: a consumer of those components provides them
# (vcpkg, source build); only nlohmann-json, needed by every consumer, is declared.
find_program(INERTIA_DPKG_DEB dpkg-deb)
if(INERTIA_DPKG_DEB)
    list(APPEND CPACK_GENERATOR DEB)
endif()
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_DEBIAN_PACKAGE_SECTION libdevel)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
set(CPACK_DEBIAN_PACKAGE_RELEASE 1)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_INERTIA_CORE_PACKAGE_NAME libinertia-core-dev)
set(CPACK_DEBIAN_INERTIA_CORE_PACKAGE_DEPENDS nlohmann-json3-dev)
set(_inertia_core_deb "libinertia-core-dev (= ${PROJECT_VERSION}-${CPACK_DEBIAN_PACKAGE_RELEASE})")
foreach(_component IN LISTS CPACK_COMPONENTS_ALL)
    if(_component STREQUAL inertia_core)
        continue()
    endif()
    string(TOUPPER ${_component} _upper)
    string(REPLACE inertia_ "" _short ${_component})
    set(CPACK_DEBIAN_${_upper}_PACKAGE_NAME libinertia-${_short}-dev)
    set(CPACK_DEBIAN_${_upper}_PACKAGE_DEPENDS "${_inertia_core_deb}")
endforeach()

include(CPack)
