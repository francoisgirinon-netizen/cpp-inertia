#!/usr/bin/env bash
# Build the line-coverage report of the library from a `coverage` workflow run
# (inside the dev container: scripts/dev tools/coverage-report.sh).
#
# Merges the raw profiles the tests wrote under build/coverage/profiles, prints the
# llvm-cov text summary and writes the HTML report to build/coverage/html/index.html.
# Covered sources: include/inertia, src and adapters/*. Dependencies, tests and build
# output are ignored.
#
# Gate: fails when the total line coverage is below INERTIA_COVERAGE_MIN_LINES (percent).
# The default below is the one versioned threshold; raise it when coverage improves
# (override per run with the environment variable).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
readonly MIN_LINES="${INERTIA_COVERAGE_MIN_LINES:-96}"
readonly TREE=build/coverage
readonly PROFILES=$TREE/profiles
readonly IGNORE='(vcpkg_installed|^/usr/|/tests/|/build/)'

shopt -s nullglob
raw=("$PROFILES"/*.profraw)
if [ "${#raw[@]}" -eq 0 ]; then
    echo "coverage-report: no profile in $PROFILES, run 'cmake --workflow --preset coverage' first" >&2
    exit 1
fi

# Every executable ctest runs (the Catch2 binaries, the conformance and server helpers).
mapfile -t executables < <(ctest --test-dir "$TREE" --show-only=json-v1 \
    | python3 -I -c '
import json, os, sys
seen = []
for test in json.load(sys.stdin)["tests"]:
    exe = test["command"][0]
    if os.path.isabs(exe) and os.access(exe, os.X_OK) and exe not in seen:
        seen.append(exe)
print("\n".join(seen))')
if [ "${#executables[@]}" -eq 0 ]; then
    echo "coverage-report: no test executable found in $TREE" >&2
    exit 1
fi

llvm-profdata merge -sparse "${raw[@]}" -o "$TREE/coverage.profdata"

objects=()
for exe in "${executables[@]:1}"; do objects+=(-object "$exe"); done

llvm-cov report "${executables[0]}" "${objects[@]}" -instr-profile="$TREE/coverage.profdata" \
    -ignore-filename-regex="$IGNORE"
llvm-cov show "${executables[0]}" "${objects[@]}" -instr-profile="$TREE/coverage.profdata" \
    -ignore-filename-regex="$IGNORE" -format=html -output-dir="$TREE/html" \
    -show-line-counts-or-regions
echo "coverage-report: $PWD/$TREE/html/index.html"

lines=$(llvm-cov export "${executables[0]}" "${objects[@]}" -instr-profile="$TREE/coverage.profdata" \
    -ignore-filename-regex="$IGNORE" -summary-only \
    | python3 -I -c 'import json, sys; print(json.load(sys.stdin)["data"][0]["totals"]["lines"]["percent"])')
if python3 -I -c 'import sys; sys.exit(not float(sys.argv[1]) < float(sys.argv[2]))' "$lines" "$MIN_LINES"; then
    printf 'coverage-report: FAIL, line coverage %.2f%% is below the required %s%%\n' "$lines" "$MIN_LINES" >&2
    exit 1
fi
printf 'coverage-report: line coverage %.2f%% (required: %s%%)\n' "$lines" "$MIN_LINES"
