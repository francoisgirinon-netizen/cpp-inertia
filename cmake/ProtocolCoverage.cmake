# Test executables whose [protocol:<id>] tags are checked by coverage_check.
# Each test target that proves protocol rules calls inertia_add_protocol_tests().
function(inertia_add_protocol_tests target)
    set_property(GLOBAL APPEND PROPERTY INERTIA_PROTOCOL_TEST_TARGETS ${target})
endfunction()

# Registers coverage_check with every target added via inertia_add_protocol_tests().
# Called deferred from the top-level CMakeLists.txt, after all directories are processed.
function(inertia_register_coverage_check python source_dir)
    get_property(_targets GLOBAL PROPERTY INERTIA_PROTOCOL_TEST_TARGETS)
    set(_args --matrix ${source_dir}/docs/protocol-coverage.toml)
    foreach(_target IN LISTS _targets)
        list(APPEND _args --tags-from $<TARGET_FILE:${_target}>)
    endforeach()
    # Optional: configure with -DINERTIA_E2E_LIST=<file> (or the environment variable of the same
    # name) with the output of `playwright test --list` to also require an e2e test for every
    # rule marked e2e = true. Playwright itself never runs here.
    if(INERTIA_E2E_LIST)
        list(APPEND _args --e2e-list ${INERTIA_E2E_LIST})
    elseif(NOT "$ENV{INERTIA_E2E_LIST}" STREQUAL "")
        list(APPEND _args --e2e-list $ENV{INERTIA_E2E_LIST})
    endif()
    if(INERTIA_COVERAGE_STRICT)
        list(APPEND _args --strict)
    endif()
    add_test(NAME coverage_check
             COMMAND ${python} ${source_dir}/tools/coverage_check.py ${_args})
endfunction()
