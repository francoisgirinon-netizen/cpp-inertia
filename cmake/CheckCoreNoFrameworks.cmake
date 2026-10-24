# Fails when the framework-neutral core references an adapter's HTTP framework (case-insensitive,
# comments included).
# Usage: cmake -DINERTIA_SOURCE_DIR=<repo> -P CheckCoreNoFrameworks.cmake
# The glob runs when the test runs, so it is not a build input.
file(GLOB_RECURSE _files "${INERTIA_SOURCE_DIR}/include/inertia/*" "${INERTIA_SOURCE_DIR}/src/*")
if(NOT _files)
    message(FATAL_ERROR "no core sources found under ${INERTIA_SOURCE_DIR}")
endif()

set(_frameworks drogon httplib)
set(_hits "")
foreach(_file IN LISTS _files)
    file(READ "${_file}" _text)
    string(TOLOWER "${_text}" _text)
    foreach(_framework IN LISTS _frameworks)
        if(_text MATCHES "${_framework}")
            file(RELATIVE_PATH _relative "${INERTIA_SOURCE_DIR}" "${_file}")
            list(APPEND _hits "${_relative} (${_framework})")
        endif()
    endforeach()
endforeach()

if(_hits)
    list(JOIN _hits "\n" _list)
    message(FATAL_ERROR "core must not depend on an adapter framework, found in:\n${_list}")
endif()
