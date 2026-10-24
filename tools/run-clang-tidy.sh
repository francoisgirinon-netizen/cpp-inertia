#!/usr/bin/env bash
# Run clang-tidy on the given files (pre-commit hook, inside the dev container).
#
# Usage: run-clang-tidy.sh FILE...   lint the given files (what the hook passes: staged ones)
#        run-clang-tidy.sh --all     lint every tracked .cpp in the main build (whole tree)
#
# clang-tidy reads the exact flags of each translation unit (include paths, defines,
# language standard) from build/dev/compile_commands.json, which CMake writes at
# configure time. So the tree must be configured, but it does not need to be built.
# When the database is missing this script configures build/dev with examples on, so
# their sources are linted too. It never runs a build. An existing tree is left alone.
set -euo pipefail
readonly TREE=build/dev

if [ "${1:-}" = "--all" ]; then
    [ "$#" -eq 1 ] || { echo "usage: $0 [--all | FILE...]" >&2; exit 2; }
    # tests/package is an out-of-tree consumer project, absent from the compile database
    mapfile -t files < <(git ls-files -- '*.cpp' ':!:tests/package/*')
    set -- "${files[@]}"
fi

[ "$#" -eq 0 ] && exit 0

if [ ! -f "$TREE/compile_commands.json" ]; then
    echo "clang-tidy: $TREE/compile_commands.json missing, configuring it (cmake --preset dev -DINERTIA_BUILD_EXAMPLES=ON, no build)" >&2
    if ! cmake --preset dev -DINERTIA_BUILD_EXAMPLES=ON; then
        echo "clang-tidy: 'cmake --preset dev' failed, so there is no compile database to lint with" >&2
        exit 1
    fi
elif ! grep -qx 'INERTIA_BUILD_EXAMPLES:BOOL=ON' "$TREE/CMakeCache.txt" 2>/dev/null; then
    echo "clang-tidy: hint: $TREE was configured without examples, so their sources are not linted; run 'scripts/dev cmake --preset dev -DINERTIA_BUILD_EXAMPLES=ON' to include them" >&2
fi

exec clang-tidy -p "$TREE" "$@"
