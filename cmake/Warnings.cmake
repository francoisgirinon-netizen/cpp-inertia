# Project-local compile flags: warnings, and the MSVC conformance switches the sources rely on.
# Link it only through $<BUILD_LOCAL_INTERFACE:inertia_warnings> (or privately from executables)
# so that neither the flags nor -Werror ever reach consumers.
#
# Every flag is selected by compiler ID and minimum compiler version, with generator expressions:
# nothing is probed with try_compile, so a flag cannot silently drop out because a probe failed.
# The same table fills INERTIA_EXPECTED_WARNING_FLAGS, the flags the current compiler must receive,
# which the CTest `flags` (tests/flags/check_effective_flags.cmake) checks against the compile
# database.
add_library(inertia_warnings INTERFACE)

# --- Supported compilers
# Declared minimums, from the C++20 library and language support that src/ relies on (concepts,
# std::span, the C++20 string members, designated initializers). Verified so far: Clang 23 and
# GCC 15 (libstdc++ 15, Linux). Apple Clang numbers its releases on its own scale: Apple Clang 16
# (Xcode 16) is based on LLVM 17.
set(_inertia_min_GNU 13)
set(_inertia_min_Clang 18)
set(_inertia_min_AppleClang 16)
set(_inertia_min_MSVC 19.40) # Visual Studio 2022 17.10
if(DEFINED _inertia_min_${CMAKE_CXX_COMPILER_ID}
   AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS _inertia_min_${CMAKE_CXX_COMPILER_ID})
    message(FATAL_ERROR
        "inertia needs ${CMAKE_CXX_COMPILER_ID} ${_inertia_min_${CMAKE_CXX_COMPILER_ID}} or newer "
        "(found ${CMAKE_CXX_COMPILER_VERSION})")
endif()

# --- The table
set(INERTIA_EXPECTED_WARNING_FLAGS)

# _inertia_warnings(<compiler id> <flag>[@<min version>]...)
# Adds each flag for that compiler ID (the CMake ID: GNU, Clang, AppleClang, MSVC), from the
# version after the '@' (none: every supported version). Records it as expected when it applies
# to the compiler in use.
function(_inertia_warnings id)
    set(expected ${INERTIA_EXPECTED_WARNING_FLAGS})
    foreach(token IN LISTS ARGN)
        string(REPLACE "@" ";" parts "${token}")
        list(GET parts 0 flag)
        list(LENGTH parts count)
        set(applies FALSE)
        if(CMAKE_CXX_COMPILER_ID STREQUAL id)
            set(applies TRUE)
        endif()
        if(count GREATER 1)
            list(GET parts 1 minimum)
            set(condition
                "$<AND:$<COMPILE_LANG_AND_ID:CXX,${id}>,$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,${minimum}>>")
            if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS minimum)
                set(applies FALSE)
            endif()
        else()
            set(condition "$<COMPILE_LANG_AND_ID:CXX,${id}>")
        endif()
        target_compile_options(inertia_warnings INTERFACE "$<${condition}:${flag}>")
        if(applies)
            list(APPEND expected ${flag})
        endif()
    endforeach()
    set(INERTIA_EXPECTED_WARNING_FLAGS ${expected} PARENT_SCOPE)
endfunction()

# Flags every GCC-compatible driver of the supported versions knows (GCC >= 13, Clang >= 18, Apple
# Clang >= 16): -Wextra-semi is GCC 8 / Clang 3.x, -Wimplicit-fallthrough GCC 7, -Wnull-dereference
# GCC 6, -Wmisleading-indentation GCC 6, -Wdouble-promotion GCC 4.6.
set(_inertia_common
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast
    -Wcast-align -Wcast-qual -Wunused -Woverloaded-virtual -Wnon-virtual-dtor
    -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough
    -Wmisleading-indentation -Wextra-semi -Wundef)

# Clang-only flags, all present since long before Clang 18: -Wshadow-all, -Wunreachable-code-
# aggressive, -Wmissing-prototypes, -Wthread-safety (Clang 3.x), -Wcomma (3.9), -Wdocumentation
# (3.4), -Wrange-loop-analysis (3.x; a group of -Wrange-loop-construct since Clang 10).
set(_inertia_clang_common
    -Wshadow-all -Wcomma -Wunreachable-code-aggressive -Wrange-loop-analysis
    -Wmissing-prototypes -Wdeprecated -Wthread-safety -Wdocumentation)

# Function effects ([[clang::nonallocating]]) arrived in Clang 20 (LLVM 20 release notes).
# INERTIA_NONALLOCATING expands to nothing without them.
_inertia_warnings(Clang ${_inertia_clang_common} ${_inertia_common} -Wfunction-effects@20)
# Apple Clang: the same flags, except -Wfunction-effects. Apple Clang 17 (Xcode 26) is based on
# LLVM 19, and the first Apple release based on LLVM 20 is not known yet: an unknown -W option
# would be an error under -Werror, so the flag is left out until a release that has it is
# confirmed on the CI (then add it here with that version).
_inertia_warnings(AppleClang ${_inertia_clang_common} ${_inertia_common})

# GCC-only: -Wlogical-op (4.3), -Wduplicated-cond (6), -Wduplicated-branches (7),
# -Wuseless-cast (4.8).
_inertia_warnings(GNU ${_inertia_common}
    -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast)

# MSVC (clang-cl reports the ID Clang and is not supported).
#   /W4                          the warning level (/Wall warns about the standard library's own
#                                headers).
#   /permissive-, /Zc:__cplusplus, /utf-8, /EHsc
#                                standard conformance, the real __cplusplus, UTF-8 sources and
#                                execution charset, standard exception handling.
#   /external:W0                 no warnings from system headers (imported targets: vcpkg).
#   /w14242 ... /w14928          warnings /W4 leaves off, from the C++ Best Practices Game list
#                                (Jason Turner), Visual Studio 2019+: narrowing (4242, 4254),
#                                hidden virtuals (4263), missing virtual dtor (4265), switch/loop
#                                and comparison traps (4287, 4289, 4296), pointer truncation (4311),
#                                comma and no-effect expressions (4545-4549, 4555), unknown pragma
#                                warning id (4619), non-thread-safe statics (4640), sign extension
#                                (4826), literal casts (4905, 4906), illegal copy-init (4928).
_inertia_warnings(MSVC /W4 /permissive- /Zc:__cplusplus /utf-8 /EHsc /external:W0
    /w14242 /w14254 /w14263 /w14265 /w14287 /we4289 /w14296 /w14311 /w14545 /w14546 /w14547
    /w14549 /w14555 /w14619 /w14640 /w14826 /w14905 /w14906 /w14928)

# --- Warnings as errors
if(MSVC)
    set(INERTIA_WERROR_FLAG /WX)
else()
    set(INERTIA_WERROR_FLAG -Werror)
endif()
if(INERTIA_WARNINGS_AS_ERRORS)
    target_compile_options(inertia_warnings INTERFACE
        "$<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang,GNU>:-Werror>"
        "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/WX>")
endif()

# Windows headers define min/max macros and pull in half of the platform: keep them out of the
# project's own translation units.
target_compile_definitions(inertia_warnings INTERFACE
    "$<$<PLATFORM_ID:Windows>:NOMINMAX;WIN32_LEAN_AND_MEAN>")
