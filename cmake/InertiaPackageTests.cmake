# Installed-package smoke tests: install to a stage, configure + build + run an external consumer
# against it. Chained with CTest fixtures, so `ctest -R package.core.run` runs its setups first.
#
# The consumers inherit the compiler, flags, build type and vcpkg setup of this build, so that
# sanitizer builds link consistently.
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
    if(CMAKE_TOOLCHAIN_FILE)
        list(APPEND _common_args
            -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
            -DVCPKG_MANIFEST_MODE=OFF
            -DVCPKG_INSTALLED_DIR=${_VCPKG_INSTALLED_DIR}
            -DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET})
    endif()

    add_test(NAME package.install
             COMMAND ${CMAKE_COMMAND} --install ${PROJECT_BINARY_DIR} --prefix ${_stage})
    set_tests_properties(package.install PROPERTIES FIXTURES_SETUP package_stage)

    # variant name, then the (comma separated) components the consumer requests
    set(_variants core "core,fake")
    if(TARGET inertia_drogon)
        list(APPEND _variants drogon "core,drogon")
    endif()
    if(TARGET inertia_httplib)
        list(APPEND _variants httplib "core,httplib")
    endif()
    list(LENGTH _variants _count)
    math(EXPR _last "${_count} - 1")
    foreach(_i RANGE 0 ${_last} 2)
        list(GET _variants ${_i} _name)
        math(EXPR _j "${_i} + 1")
        list(GET _variants ${_j} _components)
        set(_bin ${PROJECT_BINARY_DIR}/_package/consumer-${_name})
        add_test(NAME package.${_name}.configure
                 COMMAND ${CMAKE_COMMAND} -S ${_consumer_src} -B ${_bin} ${_common_args}
                         -DCMAKE_PREFIX_PATH=${_stage} -DCONSUMER_COMPONENTS=${_components})
        add_test(NAME package.${_name}.build COMMAND ${CMAKE_COMMAND} --build ${_bin})
        add_test(NAME package.${_name}.run COMMAND ${_bin}/consumer)
        set_tests_properties(package.${_name}.configure PROPERTIES
            FIXTURES_REQUIRED package_stage FIXTURES_SETUP package_${_name}_configured)
        set_tests_properties(package.${_name}.build PROPERTIES
            FIXTURES_REQUIRED package_${_name}_configured FIXTURES_SETUP package_${_name}_built)
        set_tests_properties(package.${_name}.run PROPERTIES
            FIXTURES_REQUIRED package_${_name}_built)
    endforeach()

    # A package installed without an adapter must refuse its component, clearly.
    add_test(NAME package.core_only.install
             COMMAND ${CMAKE_COMMAND} --install ${PROJECT_BINARY_DIR} --prefix ${_stage_core}
                     --component inertia_core)
    set_tests_properties(package.core_only.install PROPERTIES FIXTURES_SETUP package_stage_core)
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

    # add_subdirectory consumption: tests, examples, install rules and -Werror stay off.
    set(_sub ${PROJECT_BINARY_DIR}/_package/consumer-subproject)
    add_test(NAME package.subproject.configure
             COMMAND ${CMAKE_COMMAND} -S ${PROJECT_SOURCE_DIR}/tests/subproject -B ${_sub}
                     ${_common_args} -DINERTIA_ROOT=${PROJECT_SOURCE_DIR})
    add_test(NAME package.subproject.build COMMAND ${CMAKE_COMMAND} --build ${_sub})
    add_test(NAME package.subproject.flags
             COMMAND ${CMAKE_COMMAND} -DCOMPILE_COMMANDS=${_sub}/compile_commands.json
                     -P ${PROJECT_SOURCE_DIR}/tests/subproject/check_flags.cmake)
    set_tests_properties(package.subproject.configure PROPERTIES
        FIXTURES_SETUP package_subproject_configured)
    set_tests_properties(package.subproject.build PROPERTIES
        FIXTURES_REQUIRED package_subproject_configured FIXTURES_SETUP package_subproject_built)
    set_tests_properties(package.subproject.flags PROPERTIES
        FIXTURES_REQUIRED package_subproject_built)

    set_tests_properties(
        package.subproject.configure package.subproject.build package.subproject.flags
        package.install package.core.configure package.core.build package.core.run
        package.core_only.install package.core_only.refuses_drogon
        package.core_only.refuses_httplib
        PROPERTIES LABELS package)
    if(TARGET inertia_drogon)
        set_tests_properties(package.drogon.configure package.drogon.build package.drogon.run
                             PROPERTIES LABELS package)
    endif()
    if(TARGET inertia_httplib)
        set_tests_properties(package.httplib.configure package.httplib.build package.httplib.run
                             PROPERTIES LABELS package)
    endif()
endfunction()
