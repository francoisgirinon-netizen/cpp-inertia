add_library(inertia_warnings INTERFACE)

set(_inertia_common_warnings
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast
    -Wcast-align -Wcast-qual -Wunused -Woverloaded-virtual -Wnon-virtual-dtor
    -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough
    -Wmisleading-indentation -Wextra-semi -Wundef)

set(_inertia_clang_warnings
    -Wshadow-all -Wcomma -Wunreachable-code-aggressive -Wrange-loop-analysis
    -Wmissing-prototypes -Wdeprecated -Wthread-safety)

set(_inertia_gcc_warnings
    -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast)

target_compile_options(inertia_warnings INTERFACE ${_inertia_common_warnings})
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(inertia_warnings INTERFACE ${_inertia_clang_warnings})
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(inertia_warnings INTERFACE ${_inertia_gcc_warnings})
endif()

if(INERTIA_WARNINGS_AS_ERRORS)
    target_compile_options(inertia_warnings INTERFACE -Werror)
endif()
