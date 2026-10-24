# Fails when the framework-neutral core references Drogon (case-insensitive, comments included).
# Usage: cmake -DINERTIA_SOURCE_DIR=<repo> -P CheckCoreNoDrogon.cmake
# The glob runs when the test runs, so it is not a build input.
file(GLOB_RECURSE _files "${INERTIA_SOURCE_DIR}/include/inertia/*" "${INERTIA_SOURCE_DIR}/src/*")
if(NOT _files)
    message(FATAL_ERROR "no core sources found under ${INERTIA_SOURCE_DIR}")
endif()

set(_hits "")
foreach(_file IN LISTS _files)
    file(READ "${_file}" _text)
    string(TOLOWER "${_text}" _text)
    if(_text MATCHES "drogon")
        file(RELATIVE_PATH _relative "${INERTIA_SOURCE_DIR}" "${_file}")
        list(APPEND _hits "${_relative}")
    endif()
endforeach()

if(_hits)
    list(JOIN _hits "\n" _list)
    message(FATAL_ERROR "core must not depend on Drogon, found in:\n${_list}")
endif()
