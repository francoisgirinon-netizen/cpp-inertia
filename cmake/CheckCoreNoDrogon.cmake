# Fails when the framework-neutral core references Drogon.
# Usage: cmake -DINERTIA_SOURCE_DIR=<repo> -P CheckCoreNoDrogon.cmake
execute_process(
    COMMAND grep -rIli drogon include/inertia src
    WORKING_DIRECTORY ${INERTIA_SOURCE_DIR}
    OUTPUT_VARIABLE _matches
    ERROR_VARIABLE _errors
    RESULT_VARIABLE _status)
if(_status EQUAL 0)
    message(FATAL_ERROR "core must not depend on Drogon, found in:\n${_matches}")
elseif(NOT _status EQUAL 1)
    message(FATAL_ERROR "grep failed (${_status}): ${_errors}")
endif()
