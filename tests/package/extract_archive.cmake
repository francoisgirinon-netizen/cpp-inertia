# Extracts a CPack archive into a clean directory and checks that it holds the package files.
# Usage: cmake -DARCHIVE=<file.tar.gz> -DDEST=<dir> -DTOP=<top directory of the archive> -P ...
foreach(_var ARCHIVE DEST TOP)
    if(NOT DEFINED ${_var})
        message(FATAL_ERROR "${_var} is not set")
    endif()
endforeach()

file(REMOVE_RECURSE ${DEST})
file(MAKE_DIRECTORY ${DEST})
file(ARCHIVE_EXTRACT INPUT ${ARCHIVE} DESTINATION ${DEST})

foreach(_file IN ITEMS share/doc/inertia/LICENSE share/doc/inertia/README.md
                       lib/cmake/inertia/inertiaConfig.cmake
                       lib/cmake/inertia/inertia-core-targets.cmake)
    if(NOT EXISTS ${DEST}/${TOP}/${_file})
        message(FATAL_ERROR "${_file} is missing from the archive ${ARCHIVE}")
    endif()
endforeach()
