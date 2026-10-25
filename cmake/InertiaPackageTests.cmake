# Installed-package smoke tests: install to a stage, configure + build + run an external consumer
# against it. Chained with CTest fixtures, so `ctest -R package.core.run` runs its setups first.
#
# The consumers inherit the compiler, flags, build type and vcpkg setup of this build, so that
# sanitizer builds link consistently.

# Configure + build + run the consumer of the package staged in <stage>, as the chained tests
# package.<name>.configure/build/run. <fixture> is the fixture whose setup produces the stage; a
# name ending in 17 builds the consumer as C++17 (what the library promises), the others as C++20.
# ARGN: the arguments common to every consumer configure.
function(_inertia_consumer_tests name components stage fixture consumer_src)
    set(_bin ${PROJECT_BINARY_DIR}/_package/consumer-${name})
    # A multi-config generator (Visual Studio) builds into <bin>/<config>/ and needs --config.
    get_property(_multi GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    set(_config_dir "")
    set(_config_arg "")
    if(_multi)
        set(_config_dir "$<CONFIG>/")
        set(_config_arg --config $<CONFIG>)
    endif()
    set(_standard 20)
    if(name MATCHES "17$")
        set(_standard 17)
    endif()
    add_test(NAME package.${name}.configure
             COMMAND ${CMAKE_COMMAND} -S ${consumer_src} -B ${_bin} ${ARGN}
                     -DCMAKE_CXX_STANDARD=${_standard} -DCMAKE_CXX_STANDARD_REQUIRED=ON
                     -DCMAKE_CXX_EXTENSIONS=OFF
                     -DCMAKE_PREFIX_PATH=${stage} -DCONSUMER_COMPONENTS=${components})
    add_test(NAME package.${name}.build COMMAND ${CMAKE_COMMAND} --build ${_bin} ${_config_arg})
    add_test(NAME package.${name}.run COMMAND ${_bin}/${_config_dir}consumer)
    if(WIN32 AND _VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
        # The consumer finds the DLLs of the packages it links in the vcpkg directory of its
        # configuration (the copy next to it only holds those CMake knows as imported targets).
        set(_vcpkg_root ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET})
        set_tests_properties(package.${name}.run PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<IF:$<CONFIG:Debug>,${_vcpkg_root}/debug/bin,${_vcpkg_root}/bin>")
    endif()
    set_tests_properties(package.${name}.configure PROPERTIES
        FIXTURES_REQUIRED ${fixture} FIXTURES_SETUP package_${name}_configured)
    set_tests_properties(package.${name}.build PROPERTIES
        FIXTURES_REQUIRED package_${name}_configured FIXTURES_SETUP package_${name}_built)
    set_tests_properties(package.${name}.run PROPERTIES FIXTURES_REQUIRED package_${name}_built)
endfunction()

function(inertia_add_package_tests)
    set(_stage ${PROJECT_BINARY_DIR}/_package/stage)
    set(_stage_core ${PROJECT_BINARY_DIR}/_package/stage-core-only)
    set(_consumer_src ${PROJECT_SOURCE_DIR}/tests/package)

    set(_common_args
        -G ${CMAKE_GENERATOR}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_CXX_SCAN_FOR_MODULES=${CMAKE_CXX_SCAN_FOR_MODULES}
        "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
        "-DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}")
    if(CMAKE_GENERATOR_PLATFORM)
        list(APPEND _common_args -A ${CMAKE_GENERATOR_PLATFORM})
    endif()
    if(CMAKE_GENERATOR_TOOLSET)
        list(APPEND _common_args -T ${CMAKE_GENERATOR_TOOLSET})
    endif()
    if(CMAKE_MSVC_RUNTIME_LIBRARY)
        # The value holds generator expressions, which add_test would evaluate: keep them literal.
        string(REPLACE "$<" "$<1:$><" _runtime "${CMAKE_MSVC_RUNTIME_LIBRARY}")
        list(APPEND _common_args "-DCMAKE_MSVC_RUNTIME_LIBRARY=${_runtime}")
    endif()
    if(CMAKE_TOOLCHAIN_FILE)
        list(APPEND _common_args
            -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
            -DVCPKG_MANIFEST_MODE=OFF
            -DVCPKG_INSTALLED_DIR=${_VCPKG_INSTALLED_DIR}
            -DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET})
    endif()

    get_property(_multi GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    set(_install_config "")
    if(_multi)
        set(_install_config --config $<CONFIG>)
    endif()
    add_test(NAME package.install
             COMMAND ${CMAKE_COMMAND} --install ${PROJECT_BINARY_DIR} --prefix ${_stage}
                     ${_install_config})
    set_tests_properties(package.install PROPERTIES FIXTURES_SETUP package_stage)

    # variant name, then the (comma separated) components the consumer requests. A name ending in
    # 17 builds the consumer as C++17 (what the library promises); the others build it as C++20.
    set(_variants core "core,fake" core17 "core,fake")
    if(TARGET inertia_drogon)
        list(APPEND _variants drogon "core,drogon" drogon17 "core,drogon")
    endif()
    if(TARGET inertia_httplib)
        list(APPEND _variants httplib "core,httplib" httplib17 "core,httplib")
    endif()
    if(TARGET inertia_conformance)
        list(APPEND _variants custom "core,conformance" custom17 "core,conformance")
    endif()
    list(LENGTH _variants _count)
    math(EXPR _last "${_count} - 1")
    foreach(_i RANGE 0 ${_last} 2)
        list(GET _variants ${_i} _name)
        math(EXPR _j "${_i} + 1")
        list(GET _variants ${_j} _components)
        _inertia_consumer_tests(${_name} ${_components} ${_stage} package_stage ${_consumer_src}
                                ${_common_args})
    endforeach()

    if(MSVC AND BUILD_SHARED_LIBS)
        # The runtime guard of <inertia/attributes.hpp>: a consumer built with another C runtime
        # than the shared core (/MT here, the core is /MD or /MDd) must not link, and the error
        # names the runtime the DLL expects. The core consumer is configured as usual, only the
        # runtime differs (a later -D wins).
        set(_mismatch_bin ${PROJECT_BINARY_DIR}/_package/consumer-runtime-mismatch)
        add_test(NAME package.runtime_mismatch.configure
                 COMMAND ${CMAKE_COMMAND} -S ${_consumer_src} -B ${_mismatch_bin} ${_common_args}
                         -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
                         -DCMAKE_CXX_STANDARD=20 -DCMAKE_CXX_STANDARD_REQUIRED=ON
                         -DCMAKE_PREFIX_PATH=${_stage} -DCONSUMER_COMPONENTS=core,fake)
        add_test(NAME package.runtime_mismatch.build
                 COMMAND ${CMAKE_COMMAND} --build ${_mismatch_bin} ${_install_config})
        set_tests_properties(package.runtime_mismatch.configure PROPERTIES
            FIXTURES_REQUIRED package_stage FIXTURES_SETUP package_runtime_mismatch_configured
            LABELS package)
        # The build must fail; the output is what proves it failed for the right reason.
        set_tests_properties(package.runtime_mismatch.build PROPERTIES
            FIXTURES_REQUIRED package_runtime_mismatch_configured
            PASS_REGULAR_EXPRESSION "LNK2001[^\n]*inertia_core_requires_runtime_MT"
            LABELS package)
    endif()

    # A package installed without an adapter must refuse its component, clearly.
    add_test(NAME package.core_only.install
             COMMAND ${CMAKE_COMMAND} --install ${PROJECT_BINARY_DIR} --prefix ${_stage_core}
                     --component inertia_core ${_install_config})
    set_tests_properties(package.core_only.install PROPERTIES FIXTURES_SETUP package_stage_core)
    if(BUILD_SHARED_LIBS)
        # The core's targets file names its shared object, which is in the runtime component.
        add_test(NAME package.core_only.install_runtime
                 COMMAND ${CMAKE_COMMAND} --install ${PROJECT_BINARY_DIR} --prefix ${_stage_core}
                         --component inertia_core_runtime ${_install_config})
        set_tests_properties(package.core_only.install_runtime PROPERTIES
            FIXTURES_SETUP package_stage_core LABELS package)
    endif()
    # The adapter targets file is not part of the core component, as in a drogon-less install.
    add_test(NAME package.core_only.refuses_drogon
             COMMAND ${CMAKE_COMMAND} -S ${_consumer_src}
                     -B ${PROJECT_BINARY_DIR}/_package/consumer-refused ${_common_args}
                     -DCMAKE_PREFIX_PATH=${_stage_core} -DCONSUMER_COMPONENTS=core,drogon)
    set_tests_properties(package.core_only.refuses_drogon PROPERTIES
        FIXTURES_REQUIRED package_stage_core
        PASS_REGULAR_EXPRESSION "without the Drogon adapter")

    add_test(NAME package.core_only.refuses_httplib
             COMMAND ${CMAKE_COMMAND} -S ${_consumer_src}
                     -B ${PROJECT_BINARY_DIR}/_package/consumer-refused-httplib ${_common_args}
                     -DCMAKE_PREFIX_PATH=${_stage_core} -DCONSUMER_COMPONENTS=core,httplib)
    set_tests_properties(package.core_only.refuses_httplib PROPERTIES
        FIXTURES_REQUIRED package_stage_core
        PASS_REGULAR_EXPRESSION "without the cpp-httplib adapter")

    if(TARGET inertia_conformance)
        # The kit must really run: a consumer that compiled none of the scenarios would pass.
        foreach(_name IN ITEMS custom custom17)
            add_test(NAME package.${_name}.runs_scenarios
                     COMMAND ${PROJECT_BINARY_DIR}/_package/consumer-${_name}/consumer --list-tests)
            set_tests_properties(package.${_name}.runs_scenarios PROPERTIES
                FIXTURES_REQUIRED package_${_name}_built
                PASS_REGULAR_EXPRESSION "[1-9][0-9]* test cases"
                LABELS package)
        endforeach()
        add_test(NAME package.core_only.refuses_conformance
                 COMMAND ${CMAKE_COMMAND} -S ${_consumer_src}
                         -B ${PROJECT_BINARY_DIR}/_package/consumer-refused-conformance
                         ${_common_args} -DCMAKE_PREFIX_PATH=${_stage_core}
                         -DCONSUMER_COMPONENTS=core,conformance)
        set_tests_properties(package.core_only.refuses_conformance PROPERTIES
            FIXTURES_REQUIRED package_stage_core
            PASS_REGULAR_EXPRESSION "without the conformance kit")
        set_tests_properties(package.custom.configure package.custom.build package.custom.run
                             package.custom17.configure package.custom17.build
                             package.custom17.run package.core_only.refuses_conformance
                             PROPERTIES LABELS package)
    endif()

    # add_subdirectory consumption: tests, examples, install rules and -Werror stay off.
    set(_sub ${PROJECT_BINARY_DIR}/_package/consumer-subproject)
    string(REPLACE ";" "|" _expected_flags "${INERTIA_EXPECTED_WARNING_FLAGS}")
    add_test(NAME package.subproject.configure
             COMMAND ${CMAKE_COMMAND} -S ${PROJECT_SOURCE_DIR}/tests/subproject -B ${_sub}
                     ${_common_args} -DINERTIA_ROOT=${PROJECT_SOURCE_DIR})
    set(_sub_config "")
    if(_multi)
        set(_sub_config --config $<CONFIG>)
    endif()
    add_test(NAME package.subproject.build COMMAND ${CMAKE_COMMAND} --build ${_sub} ${_sub_config})
    # Reads the file API reply the subproject asked for (tests/subproject/CMakeLists.txt). Without
    # cmake_file_api() (older CMake) there is no reply, so no test.
    set(_sub_flags_test "")
    if(COMMAND cmake_file_api)
        set(_sub_flags_test package.subproject.flags)
        add_test(NAME package.subproject.flags
                 COMMAND ${CMAKE_COMMAND} -DMODE=subproject -DBUILD_DIR=${_sub}
                         -DCONFIG=$<CONFIG>
                         "-DEXPECTED=${_expected_flags}" -DWERROR_FLAG=${INERTIA_WERROR_FLAG}
                         -DROOT=${PROJECT_SOURCE_DIR}
                         -P ${PROJECT_SOURCE_DIR}/tests/flags/check_effective_flags.cmake)
    endif()
    set_tests_properties(package.subproject.configure PROPERTIES
        FIXTURES_SETUP package_subproject_configured)
    set_tests_properties(package.subproject.build PROPERTIES
        FIXTURES_REQUIRED package_subproject_configured FIXTURES_SETUP package_subproject_built)
    if(_sub_flags_test)
        set_tests_properties(package.subproject.flags PROPERTIES
            FIXTURES_REQUIRED package_subproject_built)
    endif()

    set_tests_properties(
        package.subproject.configure package.subproject.build ${_sub_flags_test}
        package.install package.core.configure package.core.build package.core.run
        package.core17.configure package.core17.build package.core17.run
        package.core_only.install package.core_only.refuses_drogon
        package.core_only.refuses_httplib
        PROPERTIES LABELS package)
    if(TARGET inertia_drogon)
        set_tests_properties(package.drogon.configure package.drogon.build package.drogon.run
                             package.drogon17.configure package.drogon17.build
                             package.drogon17.run PROPERTIES LABELS package)
    endif()
    if(TARGET inertia_httplib)
        set_tests_properties(package.httplib.configure package.httplib.build package.httplib.run
                             package.httplib17.configure package.httplib17.build
                             package.httplib17.run PROPERTIES LABELS package)
    endif()

    # CPack: the archive is extracted to a clean directory and consumed from there, the .deb files
    # are inspected (never installed). The generators run on the build tree, so they need the
    # project built, as every test does.
    set(_cpack_dir ${PROJECT_BINARY_DIR}/_package/cpack)
    set(_cpack_config "")
    if(_multi)
        set(_cpack_config -C $<CONFIG>)
    endif()
    add_test(NAME package.cpack.archive
             COMMAND ${CMAKE_CPACK_COMMAND} -G ${INERTIA_ARCHIVE_GENERATOR}
                     --config ${PROJECT_BINARY_DIR}/CPackConfig.cmake -B ${_cpack_dir}/archive
                     ${_cpack_config})
    set_tests_properties(package.cpack.archive PROPERTIES FIXTURES_SETUP package_cpack_packed)
    add_test(NAME package.cpack.extract
             COMMAND ${CMAKE_COMMAND} -DARCHIVE=${_cpack_dir}/archive/${INERTIA_PACKAGE_FILE_NAME}${INERTIA_ARCHIVE_SUFFIX}
                     -DDEST=${_cpack_dir}/extracted -DTOP=${INERTIA_PACKAGE_FILE_NAME}
                     -P ${_consumer_src}/extract_archive.cmake)
    set_tests_properties(package.cpack.extract PROPERTIES
        FIXTURES_REQUIRED package_cpack_packed FIXTURES_SETUP package_cpack_extracted)
    foreach(_name IN ITEMS cpack.core cpack.core17)
        _inertia_consumer_tests(${_name} "core,fake" ${_cpack_dir}/extracted/${INERTIA_PACKAGE_FILE_NAME}
                                package_cpack_extracted ${_consumer_src} ${_common_args})
        set_tests_properties(package.${_name}.configure package.${_name}.build
                             package.${_name}.run PROPERTIES LABELS package)
    endforeach()
    set_tests_properties(package.cpack.archive package.cpack.extract PROPERTIES LABELS package)

    if(INERTIA_DPKG_DEB)
        set(_deb_soversion "")
        if(BUILD_SHARED_LIBS)
            set(_deb_soversion ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
        endif()
        add_test(NAME package.cpack.deb
                 COMMAND ${CMAKE_CPACK_COMMAND} -G DEB
                         --config ${PROJECT_BINARY_DIR}/CPackConfig.cmake -B ${_cpack_dir}/deb)
        add_test(NAME package.cpack.deb.check
                 COMMAND ${CMAKE_COMMAND} -DDEB_DIR=${_cpack_dir}/deb -DVERSION=${PROJECT_VERSION}
                         -DDPKG_DEB=${INERTIA_DPKG_DEB} -DSOVERSION=${_deb_soversion}
                     -P ${_consumer_src}/check_deb.cmake)
        set_tests_properties(package.cpack.deb PROPERTIES
            FIXTURES_SETUP package_cpack_deb_packed LABELS package)
        set_tests_properties(package.cpack.deb.check PROPERTIES
            FIXTURES_REQUIRED package_cpack_deb_packed LABELS package)
    endif()

    # A hung step must fail fast instead of holding a runner: every package test has a timeout.
    # Configure, build and install steps are generous (a Visual Studio build is slow on a cold
    # runner); a consumer that only runs and exits is not.
    get_directory_property(_all_tests TESTS)
    foreach(_test IN LISTS _all_tests)
        if(_test MATCHES "^package\\.")
            set_tests_properties(${_test} PROPERTIES TIMEOUT 900)
            if(_test MATCHES "\\.(run|runs_scenarios)$")
                set_tests_properties(${_test} PROPERTIES TIMEOUT 60)
            endif()
        endif()
    endforeach()
endfunction()
