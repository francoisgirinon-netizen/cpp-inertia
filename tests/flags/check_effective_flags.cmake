# Checks the compile lines of a compile_commands.json against the warning flags the build is
# supposed to apply (cmake/Warnings.cmake builds the list: INERTIA_EXPECTED_WARNING_FLAGS).
#
#   cmake -DMODE=<project|subproject> -DCOMPILE_COMMANDS=<compile_commands.json>
#         -DEXPECTED=<flags, separated by '|'> -DWERROR=<ON|OFF> -DWERROR_FLAG=<flag>
#         -DROOT=<source directory of inertia> -P check_effective_flags.cmake
#
# project:    the library sources (src/, adapters/*/src/) carry every expected flag, and -Werror
#             when INERTIA_WARNINGS_AS_ERRORS is on.
# subproject: the library sources carry every expected flag but never -Werror, and the consumer
#             (tests/package/main_core.cpp) carries none of them and no -Werror.
foreach(_var MODE COMPILE_COMMANDS EXPECTED ROOT WERROR_FLAG)
    if(NOT DEFINED ${_var})
        message(FATAL_ERROR "${_var} is not set")
    endif()
endforeach()
string(REPLACE "|" ";" _expected "${EXPECTED}")
if(NOT _expected)
    message(FATAL_ERROR "no expected warning flag: the flag table is empty for this compiler")
endif()

file(READ "${COMPILE_COMMANDS}" _json)
string(JSON _count LENGTH "${_json}")
math(EXPR _last "${_count} - 1")

function(has_flag command flag out)
    string(REGEX REPLACE "([][+.*^$()?|\\\\])" "\\\\\\1" _quoted "${flag}")
    if(command MATCHES "(^| )${_quoted}( |$)")
        set(${out} TRUE PARENT_SCOPE)
    else()
        set(${out} FALSE PARENT_SCOPE)
    endif()
endfunction()

set(_library_checked 0)
set(_consumer_checked 0)
foreach(_i RANGE 0 ${_last})
    string(JSON _file GET "${_json}" ${_i} file)
    string(JSON _command GET "${_json}" ${_i} command)
    if(_file MATCHES "^${ROOT}/(src/|adapters/[^/]+/src/)")
        set(_role library)
    elseif(_file MATCHES "^${ROOT}/tests/package/" AND MODE STREQUAL "subproject")
        set(_role consumer)
    else()
        continue()
    endif()
    if(_role STREQUAL library)
        math(EXPR _library_checked "${_library_checked} + 1")
        foreach(_flag IN LISTS _expected)
            has_flag("${_command}" "${_flag}" _found)
            if(NOT _found)
                message(FATAL_ERROR "${_file} is compiled without ${_flag}:\n${_command}")
            endif()
        endforeach()
        has_flag("${_command}" "${WERROR_FLAG}" _werror)
        if(MODE STREQUAL "project" AND WERROR AND NOT _werror)
            message(FATAL_ERROR "${_file} is compiled without ${WERROR_FLAG}:\n${_command}")
        endif()
        if(MODE STREQUAL "subproject" AND _werror)
            message(FATAL_ERROR "${WERROR_FLAG} leaked into a subproject build: ${_file}")
        endif()
    else()
        math(EXPR _consumer_checked "${_consumer_checked} + 1")
        foreach(_flag IN LISTS _expected WERROR_FLAG)
            has_flag("${_command}" "${_flag}" _found)
            if(_found)
                message(FATAL_ERROR "${_flag} leaked into the consumer ${_file}:\n${_command}")
            endif()
        endforeach()
    endif()
endforeach()

if(_library_checked EQUAL 0)
    message(FATAL_ERROR "no library source found in ${COMPILE_COMMANDS}")
endif()
if(MODE STREQUAL "subproject" AND _consumer_checked EQUAL 0)
    message(FATAL_ERROR "no consumer source found in ${COMPILE_COMMANDS}")
endif()
message(STATUS "${_library_checked} library and ${_consumer_checked} consumer compile lines checked")
