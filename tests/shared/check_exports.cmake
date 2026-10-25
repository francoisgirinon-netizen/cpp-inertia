# Checks the dynamic symbol table of a shared library of the project (Linux, ELF).
#
#   cmake -DNM=<nm> -DLIBRARY=<libinertia_x.so> -DREQUIRED=<symbol prefixes, separated by '|'>
#         -P check_exports.cmake
#
# 1. Every defined symbol belongs to namespace inertia (functions and variables, and the vtable,
#    VTT and typeinfo of its classes). The std:: and nlohmann:: template instantiations that the
#    library holds are not exported: cmake/inertia.map and the hidden visibility see to it.
# 2. Each REQUIRED prefix names a public symbol (demangled) that must be there: the export macros
#    did their job.
foreach(_var NM LIBRARY REQUIRED)
    if(NOT DEFINED ${_var})
        message(FATAL_ERROR "${_var} is not set")
    endif()
endforeach()
string(REPLACE "|" ";" _required "${REQUIRED}")

execute_process(COMMAND ${NM} -D --defined-only --demangle ${LIBRARY}
                OUTPUT_VARIABLE _symbols RESULT_VARIABLE _status ERROR_VARIABLE _error)
if(NOT _status EQUAL 0)
    message(FATAL_ERROR "${NM} -D ${LIBRARY} failed: ${_error}")
endif()
string(REPLACE "\n" ";" _lines "${_symbols}")

# Symbols the linker itself adds to every shared object.
set(_linker_symbols _init _fini __bss_start _edata _end)

set(_names)
set(_foreign)
foreach(_line IN LISTS _lines)
    if(NOT _line MATCHES "^[0-9a-fA-F]+ [A-Za-z] (.+)$")
        continue()
    endif()
    set(_name "${CMAKE_MATCH_1}")
    list(APPEND _names "${_name}")
    if(_name IN_LIST _linker_symbols)
        continue()
    endif()
    if(NOT _name MATCHES "^((typeinfo for|typeinfo name for|vtable for|VTT for) )?inertia::")
        list(APPEND _foreign "${_name}")
    endif()
endforeach()
list(LENGTH _names _count)
if(_count EQUAL 0)
    message(FATAL_ERROR "no symbol found in ${LIBRARY}: is nm able to read it?")
endif()
if(_foreign)
    list(JOIN _foreign "\n  " _listing)
    message(FATAL_ERROR "${LIBRARY} exports symbols outside namespace inertia:\n  ${_listing}")
endif()

foreach(_prefix IN LISTS _required)
    set(_found FALSE)
    foreach(_name IN LISTS _names)
        string(FIND "${_name}" "${_prefix}" _at)
        if(_at EQUAL 0)
            set(_found TRUE)
            break()
        endif()
    endforeach()
    if(NOT _found)
        message(FATAL_ERROR "${LIBRARY} does not export '${_prefix}'")
    endif()
endforeach()
message(STATUS "${_count} symbols, all in namespace inertia; ${_required} present")
