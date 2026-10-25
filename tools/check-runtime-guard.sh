#!/usr/bin/env bash
# Windows CI: check which inertia_core_requires_runtime_* symbols a build exports
# (see <inertia/attributes.hpp> and src/runtime_guard.cpp).
#
#   tools/check-runtime-guard.sh <build-dir> <Debug|Release> <shared|static>
#
# shared: every core DLL of that configuration (the build's own and the copies placed next to the
#         tests) exports exactly one such function, MDd for Debug and MD for Release.
# static: neither the core's import/static library nor any DLL contains one (the guard only exists
#         in a shared build).
# Needs dumpbin on PATH (a Visual Studio developer prompt) or the Visual Studio installer's vswhere.
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <build-dir> <Debug|Release> <shared|static>" >&2
    exit 2
fi
build=$1
config=$2
linkage=$3

find_dumpbin() {
    if command -v dumpbin > /dev/null 2>&1; then
        command -v dumpbin
        return
    fi
    local vswhere="/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
    [ -x "$vswhere" ] || { echo "dumpbin not found and no vswhere" >&2; return 1; }
    local install
    install=$("$vswhere" -latest -products '*' -property installationPath | tr -d '\r')
    local tool
    tool=$(ls "$(cygpath -u "$install")"/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe | sort | tail -n 1)
    [ -x "$tool" ] || { echo "dumpbin.exe not found under $install" >&2; return 1; }
    echo "$tool"
}
dumpbin=$(find_dumpbin)

# The files of this configuration only: a multi-config build tree holds Debug and Release side by side.
# The package tests' stages and consumers (_package) are left out: they are built from the same files.
mapfile -t dlls < <(find "$build" -ipath "*/$config/*" -not -ipath '*/_package/*' -iname 'inertia_core*.dll' | sort)
mapfile -t libs < <(find "$build" -ipath "*/$config/*" -not -ipath '*/_package/*' -iname 'inertia_core*.lib' | sort)

symbols() {
    # -exports lists a DLL's export table; -symbols reads a static library. (-flag, not /flag: MSYS
    # would turn /exports into a path.)
    "$dumpbin" "$@" | tr -d '\r' | grep -o 'inertia_core_requires_runtime_[A-Za-z]*' | sort -u || true
}

case $linkage in
shared)
    if [ "${#dlls[@]}" -eq 0 ]; then
        echo "no inertia_core DLL for $config under $build" >&2
        exit 1
    fi
    case $config in
    Debug) expected=inertia_core_requires_runtime_MDd ;;
    Release) expected=inertia_core_requires_runtime_MD ;;
    *) echo "unknown configuration $config" >&2; exit 2 ;;
    esac
    for dll in "${dlls[@]}"; do
        found=$(symbols -exports "$dll")
        echo "$dll exports: ${found:-nothing}"
        if [ "$found" != "$expected" ]; then
            echo "expected exactly one runtime guard, $expected" >&2
            exit 1
        fi
    done
    ;;
static)
    if [ "${#dlls[@]}" -ne 0 ]; then
        echo "a static build must not produce an inertia_core DLL: ${dlls[*]}" >&2
        exit 1
    fi
    if [ "${#libs[@]}" -eq 0 ]; then
        echo "no inertia_core library for $config under $build" >&2
        exit 1
    fi
    for lib in "${libs[@]}"; do
        found=$(symbols -symbols "$lib")
        echo "$lib contains: ${found:-nothing}"
        if [ -n "$found" ]; then
            echo "a static build must have no runtime guard, found: $found" >&2
            exit 1
        fi
    done
    ;;
*)
    echo "unknown linkage $linkage" >&2
    exit 2
    ;;
esac
