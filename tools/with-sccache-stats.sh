#!/usr/bin/env bash
# Runs the command, then prints the sccache statistics whether it passed or not (the CI jobs read
# the cache hit rate from the log of a failed build too). Exits with the command's status.
set -uo pipefail
status=0
"$@" || status=$?
sccache --show-stats
exit "$status"
