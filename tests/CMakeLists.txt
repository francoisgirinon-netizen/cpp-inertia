# The tests themselves are C++20; consumers of the library only need C++17 (see headers_cxx17
# and the package.*17 tests).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Catch2 CONFIG REQUIRED)
include(Catch)
# Discover tests when ctest starts, not by running every test executable at build time (which
# would start the Drogon test server, also under the sanitizers).
set(CMAKE_CATCH_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST)

add_subdirectory(support)
add_subdirectory(core)
if(INERTIA_BUILD_DROGON)
    add_subdirectory(drogon) # before conformance: it defines the Drogon test support
endif()
if(INERTIA_BUILD_HTTPLIB)
    add_subdirectory(httplib) # before conformance: it defines the httplib test support
endif()
add_subdirectory(conformance)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
# Deferred so that every test target registered in later subdirectories is included. It runs in the
# top-level directory scope, where Python3_EXECUTABLE is not visible: expand it now.
cmake_language(EVAL CODE "
    cmake_language(DEFER DIRECTORY [[${PROJECT_SOURCE_DIR}]] CALL inertia_register_coverage_check
                   [[${Python3_EXECUTABLE}]] [[${PROJECT_SOURCE_DIR}]])
")

# The Python tools' own tests (coverage_check, version watch, the generated README table).
add_test(NAME tools_unit_tests
         COMMAND ${Python3_EXECUTABLE} -m unittest discover -s tools/tests
         WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
set_tests_properties(tools_unit_tests PROPERTIES LABELS meta)

if(INERTIA_INSTALL)
    include(InertiaPackageTests)
    inertia_add_package_tests()
endif()

# The warning flags of cmake/Warnings.cmake reach the library's own compile lines. The test reads
# the CMake file API reply (every generator writes it, Visual Studio included), which this
# configure asks for: cmake_file_api() is newer than the CMake minimum of this project, so the
# flags tests exist only with a CMake that has it (the pinned one, 4.4.4, does).
if(COMMAND cmake_file_api)
    cmake_file_api(QUERY API_VERSION 1 CODEMODEL 2.0)
    string(REPLACE ";" "|" _expected_flags "${INERTIA_EXPECTED_WARNING_FLAGS}")
    add_test(NAME flags_applied
             COMMAND ${CMAKE_COMMAND} -DMODE=project -DBUILD_DIR=${CMAKE_BINARY_DIR}
                     -DCONFIG=$<CONFIG>
                     "-DEXPECTED=${_expected_flags}" -DWERROR=${INERTIA_WARNINGS_AS_ERRORS}
                     -DWERROR_FLAG=${INERTIA_WERROR_FLAG} -DROOT=${PROJECT_SOURCE_DIR}
                     -P ${PROJECT_SOURCE_DIR}/tests/flags/check_effective_flags.cmake)
    set_tests_properties(flags_applied PROPERTIES LABELS meta)
endif()

# A shared library exports its public API and nothing else (no std::, no nlohmann::). Linux only:
# the check reads the ELF dynamic symbol table.
if(BUILD_SHARED_LIBS AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_program(INERTIA_NM NAMES llvm-nm nm REQUIRED)
    set(_exports_core "inertia::Inertia::page(|inertia::dumpJson(|inertia::headers::iequals(|inertia::Prop::Prop(|vtable for inertia::DiskFileReader|typeinfo for inertia::FileReader")
    set(_exports_fake "inertia::fake::MemorySession::get|vtable for inertia::fake::FakeRequest|typeinfo for inertia::RequestView")
    set(_exports_drogon "inertia::drogon::install(|vtable for inertia::drogon::DrogonRequest")
    set(_exports_httplib "inertia::httplib::install(|vtable for inertia::httplib::HttplibRequest")
    foreach(_library IN ITEMS core fake drogon httplib)
        if(TARGET inertia_${_library})
            add_test(NAME exports.${_library}
                     COMMAND ${CMAKE_COMMAND} -DNM=${INERTIA_NM}
                             -DLIBRARY=$<TARGET_FILE:inertia_${_library}>
                             "-DREQUIRED=${_exports_${_library}}"
                             -P ${PROJECT_SOURCE_DIR}/tests/shared/check_exports.cmake)
            set_tests_properties(exports.${_library} PROPERTIES LABELS meta)
        endif()
    endforeach()
endif()

add_test(NAME core_has_no_adapter_frameworks
         COMMAND ${CMAKE_COMMAND} -DINERTIA_SOURCE_DIR=${PROJECT_SOURCE_DIR}
                 -P ${PROJECT_SOURCE_DIR}/cmake/CheckCoreNoFrameworks.cmake)
set_tests_properties(core_has_no_adapter_frameworks PROPERTIES LABELS meta)

# Every public header must compile on its own (the verify targets are not part of `all`).
# A multi-config generator (Visual Studio) builds the configuration the test runs with.
get_property(_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
set(_build_config "")
if(_multi_config)
    set(_build_config --config $<CONFIG>)
endif()
add_test(NAME headers_self_contained
         COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} ${_build_config}
                 --target all_verify_interface_header_sets)
set_tests_properties(headers_self_contained PROPERTIES LABELS meta)

# Consumers only need C++17: one translation unit that includes every public header (the file sets
# of the library targets that are built) must compile as strict C++17 (the library's own sources
# stay C++20; the conformance kit's sources are built as C++17 by the in-tree conformance
# executables). Each header compiling on its own is headers_self_contained's job. The object
# library is not part of `all`; the test builds it, with the project warnings and -Werror.
set(_cxx17_includes)
set(_cxx17_targets)
foreach(_library IN ITEMS core fake drogon httplib conformance)
    if(NOT TARGET inertia_${_library})
        continue() # an adapter that is not built (core-only preset) has no headers to check
    endif()
    list(APPEND _cxx17_targets inertia::${_library})
    get_target_property(_dirs inertia_${_library} HEADER_DIRS_HEADERS)
    get_target_property(_files inertia_${_library} HEADER_SET_HEADERS)
    foreach(_file IN LISTS _files)
        foreach(_dir IN LISTS _dirs)
            if(_file MATCHES "^${_dir}/")
                cmake_path(RELATIVE_PATH _file BASE_DIRECTORY ${_dir} OUTPUT_VARIABLE _include)
                list(APPEND _cxx17_includes "#include <${_include}>")
                break()
            endif()
        endforeach()
    endforeach()
endforeach()
list(JOIN _cxx17_includes "\n" _cxx17_content)
set(_cxx17_unit ${CMAKE_CURRENT_BINARY_DIR}/headers_cxx17.cpp)
file(CONFIGURE OUTPUT ${_cxx17_unit} CONTENT "@_cxx17_content@\n" @ONLY)
# Kept out of the compile database: clang-tidy takes the first entry of a file, and the C++17
# unit of this test must not be what it judges the kit and the headers with.
add_library(inertia_headers_cxx17 OBJECT EXCLUDE_FROM_ALL ${_cxx17_unit})
set_target_properties(inertia_headers_cxx17 PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON
                                                       CXX_EXTENSIONS OFF
                                                       EXPORT_COMPILE_COMMANDS OFF)
target_link_libraries(inertia_headers_cxx17 PRIVATE ${_cxx17_targets} inertia_warnings)
target_compile_options(inertia_headers_cxx17 PRIVATE ${INERTIA_WERROR_FLAG})
add_test(NAME headers_cxx17
         COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} ${_build_config}
                 --target inertia_headers_cxx17)
set_tests_properties(headers_cxx17 PROPERTIES LABELS meta)
