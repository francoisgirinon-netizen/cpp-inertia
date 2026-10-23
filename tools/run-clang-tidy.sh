#!/usr/bin/env bash
# Run clang-tidy on the given files using build/dev/compile_commands.json.
set -euo pipefail
if [ ! -f build/dev/compile_commands.json ]; then
    echo "clang-tidy: build/dev/compile_commands.json missing (run 'cmake --preset dev'), skipping"
    exit 0
fi
[ "$#" -eq 0 ] && exit 0
exec clang-tidy -p build/dev "$@"
