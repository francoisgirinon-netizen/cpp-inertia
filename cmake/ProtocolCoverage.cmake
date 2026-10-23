# Test executables whose [protocol:<id>] tags are checked by coverage_check.
# Each test target that proves protocol rules calls inertia_add_protocol_tests().
function(inertia_add_protocol_tests target)
    set_property(GLOBAL APPEND PROPERTY INERTIA_PROTOCOL_TEST_TARGETS ${target})
endfunction()
