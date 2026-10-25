# Checks the control fields and the contents of the .deb files CPack produced.
# Usage: cmake -DDEB_DIR=<dir> -DVERSION=<x.y.z> -DDPKG_DEB=<dpkg-deb> [-DSOVERSION=<x.y>] -P ...
# SOVERSION is set for a shared build: it then also has one runtime package per library.
foreach(_var DEB_DIR VERSION DPKG_DEB)
    if(NOT DEFINED ${_var})
        message(FATAL_ERROR "${_var} is not set")
    endif()
endforeach()

function(deb_field deb field out)
    execute_process(COMMAND ${DPKG_DEB} --field ${deb} ${field}
                    OUTPUT_VARIABLE _value RESULT_VARIABLE _status
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT _status EQUAL 0)
        message(FATAL_ERROR "dpkg-deb --field ${deb} ${field} failed")
    endif()
    set(${out} "${_value}" PARENT_SCOPE)
endfunction()

function(expect deb field value)
    deb_field(${deb} ${field} _actual)
    if(NOT _actual STREQUAL value)
        message(FATAL_ERROR "${deb}: ${field} is '${_actual}', expected '${value}'")
    endif()
endfunction()

file(GLOB _debs ${DEB_DIR}/libinertia-*-dev_*.deb)
file(GLOB _core ${DEB_DIR}/libinertia-core-dev_*.deb)
list(LENGTH _core _core_count)
if(NOT _core_count EQUAL 1)
    message(FATAL_ERROR "expected one libinertia-core-dev .deb in ${DEB_DIR}, found '${_core}'")
endif()

expect(${_core} Package libinertia-core-dev)
expect(${_core} Version ${VERSION}-1)
expect(${_core} Maintainer "cpp-inertia maintainers")
expect(${_core} Section libdevel)
if(SOVERSION)
    expect(${_core} Depends "nlohmann-json3-dev, libinertia-core${SOVERSION} (= ${VERSION}-1)")
    foreach(_runtime IN ITEMS core)
        file(GLOB _runtime_deb ${DEB_DIR}/libinertia-${_runtime}${SOVERSION}_*.deb)
        list(LENGTH _runtime_deb _runtime_count)
        if(NOT _runtime_count EQUAL 1)
            message(FATAL_ERROR "expected one libinertia-${_runtime}${SOVERSION} .deb, found '${_runtime_deb}'")
        endif()
        execute_process(COMMAND ${DPKG_DEB} --contents ${_runtime_deb}
                        OUTPUT_VARIABLE _runtime_contents RESULT_VARIABLE _status)
        if(NOT _status EQUAL 0 OR NOT _runtime_contents MATCHES "libinertia_${_runtime}\\.so\\.${SOVERSION}")
            message(FATAL_ERROR "${_runtime_deb} lacks the versioned shared object:\n${_runtime_contents}")
        endif()
    endforeach()
else()
    expect(${_core} Depends nlohmann-json3-dev)
endif()

execute_process(COMMAND ${DPKG_DEB} --contents ${_core}
                OUTPUT_VARIABLE _contents RESULT_VARIABLE _status)
if(NOT _status EQUAL 0)
    message(FATAL_ERROR "dpkg-deb --contents ${_core} failed")
endif()
foreach(_file IN ITEMS share/doc/inertia/LICENSE include/inertia/
                       lib/cmake/inertia/inertiaConfig.cmake)
    string(FIND "${_contents}" "${_file}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "${_core} does not contain ${_file}:\n${_contents}")
    endif()
endforeach()

# Every other package depends on exactly this version of the core one.
foreach(_deb IN LISTS _debs)
    if(NOT _deb STREQUAL _core)
        string(REGEX REPLACE "^.*/libinertia-(.*)-dev_.*$" "\\1" _short ${_deb})
        # The conformance kit has no library of its own, so no runtime package either.
        if(SOVERSION AND NOT _short STREQUAL conformance)
            expect(${_deb} Depends
                   "libinertia-core-dev (= ${VERSION}-1), libinertia-${_short}${SOVERSION} (= ${VERSION}-1)")
        else()
            expect(${_deb} Depends "libinertia-core-dev (= ${VERSION}-1)")
        endif()
        expect(${_deb} Maintainer "cpp-inertia maintainers")
    endif()
endforeach()
