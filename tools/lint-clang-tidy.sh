#!/usr/bin/env bash
# Run the clang-tidy pre-commit hook on the whole tree through ctcache
# (inside the dev container: scripts/dev tools/lint-clang-tidy.sh).
#
# ctcache (https://github.com/matus-chochlik/ctcache, `clang-tidy-cache`) hashes the preprocessed
# translation unit, the effective .clang-tidy configuration and the flags of each file, and skips
# clang-tidy when that exact input already passed. A header edit changes the preprocessed text of
# every .cpp that includes it, a .clang-tidy edit changes the dumped configuration: both miss.
#
# The cpp-linter-hooks hook runs the clang-tidy executable of its own pre-commit environment, not
# the first one on PATH, so a PATH wrapper would never be called. The script puts the wrapper in
# that environment instead, in place of the `clang-tidy` entry point (the original stays next to it
# as clang-tidy.real).
#
# Environment:
#   CTCACHE_SRC       checkout of ctcache (default: build/ctcache; CI checks it out at a pinned SHA)
#   CTCACHE_DIR       cache directory (default: build/ctcache-data); CI persists it with actions/cache
#   PRE_COMMIT_HOME   pre-commit environments (default: build/pre-commit-home)
#
# Writes build/clang-tidy.log (the diagnostics, paths relative to the repository) and
# build/clang-tidy-summary.md (counts and ctcache statistics; CI appends it to the job summary).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
root=$PWD
export CTCACHE_SRC="${CTCACHE_SRC:-$root/build/ctcache}"
export CTCACHE_DIR="${CTCACHE_DIR:-$root/build/ctcache-data}"
export PRE_COMMIT_HOME="${PRE_COMMIT_HOME:-$root/build/pre-commit-home}"
readonly PRE_COMMIT_URL=https://github.com/pre-commit/pre-commit/releases/download/v4.7.0/pre-commit-4.7.0.pyz
readonly PRE_COMMIT_SHA256=393488513aed286d7a5a978a1fe79b2f5a8802927242a2d241ff10c1a8e8c885
readonly pyz=$root/build/pre-commit-4.7.0.pyz
readonly log=$root/build/clang-tidy.log
readonly summary=$root/build/clang-tidy-summary.md
readonly cache_main=$CTCACHE_SRC/src/ctcache/clang_tidy_cache.py

[ -f "$cache_main" ] || { echo "ctcache not found: $cache_main" >&2; exit 2; }
[ -f build/dev/compile_commands.json ] || { echo "configure the dev preset first" >&2; exit 2; }

# The dev image has no pip and no venv module: pre-commit comes as its zipapp release (which bundles
# virtualenv), verified against the pinned checksum.
if ! echo "$PRE_COMMIT_SHA256  $pyz" | sha256sum --check --status 2> /dev/null; then
    mkdir -p "$(dirname "$pyz")"
    curl --fail --silent --show-error --location --output "$pyz" "$PRE_COMMIT_URL"
    echo "$PRE_COMMIT_SHA256  $pyz" | sha256sum --check --status
fi
pre-commit() { python3 "$pyz" "$@"; }

python3 tools/host-compile-commands.py

# Create the hook's environment without running it (no file matches), install the clang-tidy wheel
# the hook would install on its first run, then wrap it.
version=$(sed -n 's/.*--version=\([0-9][0-9.]*\).*/\1/p' .pre-commit-config.yaml)
[ -n "$version" ] || { echo "no clang-tidy --version in .pre-commit-config.yaml" >&2; exit 2; }
pre-commit run clang-tidy --files .clang-tidy > /dev/null
wrapped=0
for hook in "$PRE_COMMIT_HOME"/repo*/py_env-*/bin/clang-tidy-hook; do
    [ -e "$hook" ] || continue
    bin=$(dirname "$hook")
    "$bin/python" -m pip install --quiet --disable-pip-version-check "clang-tidy==$version"
    if [ ! -e "$bin/clang-tidy.real" ]; then
        mv "$bin/clang-tidy" "$bin/clang-tidy.real"
    fi
    cat > "$bin/clang-tidy" <<WRAPPER
#!/bin/sh
exec python3 "$cache_main" "$bin/clang-tidy.real" "\$@"
WRAPPER
    chmod +x "$bin/clang-tidy"
    wrapped=$((wrapped + 1))
done
[ "$wrapped" -gt 0 ] || { echo "no cpp-linter-hooks environment to wrap" >&2; exit 2; }

python3 "$cache_main" --zero-stats
status=0
start=$(date +%s)
# /src is the repository inside the container: strip it so that the paths are relative to the
# workspace, which is what the GitHub problem matcher turns into annotations.
pre-commit run clang-tidy --all-files --show-diff-on-failure 2>&1 | sed -u -E 's#^/src/##' | tee "$log" \
    || status=$?
elapsed=$(($(date +%s) - start))

stats=$(python3 "$cache_main" --print-stats)
hits=$(python3 -c 'import json,sys; print(json.load(sys.stdin)["local"]["hit_count"])' <<< "$stats")
misses=$(python3 -c 'import json,sys; print(json.load(sys.stdin)["local"]["miss_count"])' <<< "$stats")
diagnostics=$(grep -cE '^[^ ]+:[0-9]+:[0-9]+: (warning|error):' "$log" || true)
line="clang-tidy: $diagnostics diagnostics, ctcache $hits hits / $misses misses, ${elapsed}s"
echo "$line"
{
    echo "### clang-tidy"
    echo
    echo "| Diagnostics | ctcache hits | ctcache misses | Time |"
    echo "| ---: | ---: | ---: | ---: |"
    echo "| $diagnostics | $hits | $misses | ${elapsed}s |"
} > "$summary"
exit "$status"
