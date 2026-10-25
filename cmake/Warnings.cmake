# Project-local warning flags. Link it only through $<BUILD_LOCAL_INTERFACE:inertia_warnings> (or
# privately from executables) so that neither the flags nor -Werror ever reach consumers.
add_library(inertia_warnings INTERFACE)

set(_inertia_common_warnings
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast
    -Wcast-align -Wcast-qual -Wunused -Woverloaded-virtual -Wnon-virtual-dtor
    -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough
    -Wmisleading-indentation -Wextra-semi -Wundef)

set(_inertia_clang_warnings
    -Wshadow-all -Wcomma -Wunreachable-code-aggressive -Wrange-loop-analysis
    -Wmissing-prototypes -Wdeprecated -Wthread-safety -Wdocumentation)

# Function effects ([[clang::nonallocating]]) exist from Clang 20; older compilers do not know the
# flag, which -Werror would turn into a failure. INERTIA_NONALLOCATING expands to nothing there.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wfunction-effects INERTIA_HAS_WFUNCTION_EFFECTS)
if(INERTIA_HAS_WFUNCTION_EFFECTS)
    list(APPEND _inertia_clang_warnings -Wfunction-effects)
endif()

set(_inertia_gcc_warnings
    -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast)

target_compile_options(inertia_warnings INTERFACE
    "$<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang,GNU>:${_inertia_common_warnings}>"
    "$<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>:${_inertia_clang_warnings}>"
    "$<$<COMPILE_LANG_AND_ID:CXX,GNU>:${_inertia_gcc_warnings}>"
    "$<$<AND:$<BOOL:${INERTIA_WARNINGS_AS_ERRORS}>,$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang,GNU>>:-Werror>")
