#!/usr/bin/env python3
"""Make the container compile database usable by clangd on the host.

CMake writes build/dev/compile_commands.json inside the dev container, where the repository is
mounted at /src. Its paths are therefore container paths, which host tools (clangd, Serena, IDEs)
cannot open. This script copies that database to build/dev/host/compile_commands.json with the
container root rewritten to the host root. Nothing else changes: the build is not touched.

Run it on the host after configuring the container tree:

    scripts/dev cmake --preset dev
    python3 tools/host-compile-commands.py
"""

import argparse
import hashlib
import json
import shlex
import sys
from pathlib import Path

DEFAULT_CONTAINER_ROOT = "/src"
REPO_ROOT = Path(__file__).resolve().parent.parent

# Options whose value is the next argument (-I /src/x) or the rest of the same argument (-I/src/x).
PATH_FLAGS = ("-I", "-isystem", "-iquote", "-idirafter", "-F", "-include", "--include=", "-o")
# Options whose value is the next argument only, and the joined form with "=".
SEPARATE_PATH_FLAGS = ("-I", "-isystem", "-iquote", "-idirafter", "-F", "-include", "-o")
# Flags that name a toolchain location of the container's conda clang. Host clang finds its own
# toolchain, so these are dropped (both the joined and the separate form).
DROPPED_FLAGS = ("--gcc-install-dir", "--sysroot")

RESPONSE_FILE_WARNING = "warning: response file {} not found on the host, its path is rewritten but its content is not"


class InputMissing(Exception):
    pass


def under_root(path, root):
    """True when path is root itself or a path below it ('/srcfoo' is not below '/src')."""
    return path == root or path.startswith(root.rstrip("/") + "/")


def rewrite_path(path, container_root, host_root):
    if under_root(path, container_root):
        return host_root.rstrip("/") + path[len(container_root.rstrip("/")):]
    return path


def is_dropped(arg):
    for flag in DROPPED_FLAGS:
        if arg == flag or arg.startswith(flag + "="):
            return True
    return False


class Rewriter:
    def __init__(self, container_root, host_root, rsp_dir, warnings):
        self.container_root = container_root
        self.host_root = host_root
        self.rsp_dir = rsp_dir
        self.warnings = warnings
        self._rsp_done = {}

    def rewrite_path(self, path):
        return rewrite_path(path, self.container_root, self.host_root)

    def rewrite_response_file(self, arg):
        """'@/src/build/x.rsp' -> '@<output>/rsp/<digest>-x.rsp' with its paths rewritten."""
        original = arg[1:]
        if original in self._rsp_done:
            return "@" + self._rsp_done[original]
        host_path = Path(self.rewrite_path(original))
        digest = hashlib.sha1(original.encode()).hexdigest()[:12]
        out = self.rsp_dir / f"{digest}-{host_path.name}"
        if host_path.is_file():
            tokens = shlex.split(host_path.read_text(encoding="utf-8"))
            rewritten = self.rewrite_arguments(tokens)
            self.rsp_dir.mkdir(parents=True, exist_ok=True)
            out.write_text("\n".join(shlex.quote(token) for token in rewritten) + "\n", encoding="utf-8")
            target = out.as_posix()
        else:
            self.warnings.append(RESPONSE_FILE_WARNING.format(host_path))
            target = host_path.as_posix()
        self._rsp_done[original] = target
        return "@" + target

    def rewrite_arguments(self, arguments):
        result = []
        skip_next = False
        previous = None
        for index, arg in enumerate(arguments):
            if skip_next:
                skip_next = False
                previous = None
                continue
            if is_dropped(arg):
                if arg in DROPPED_FLAGS:  # separate form: the value is the next argument
                    skip_next = True
                previous = None
                continue
            if arg.startswith("@") and len(arg) > 1:
                result.append(self.rewrite_response_file(arg))
            elif previous in SEPARATE_PATH_FLAGS:
                result.append(self.rewrite_path(arg))
            else:
                result.append(self._rewrite_joined(arg))
            previous = arg
        return result

    def _rewrite_joined(self, arg):
        if under_root(arg, self.container_root):
            return self.rewrite_path(arg)
        for flag in PATH_FLAGS:
            if flag == "--include=":
                if arg.startswith(flag):
                    return flag + self.rewrite_path(arg[len(flag):])
            elif arg.startswith(flag) and len(arg) > len(flag):
                value = arg[len(flag):]
                if under_root(value, self.container_root):
                    return flag + self.rewrite_path(value)
        return arg


def convert_entry(entry, rewriter):
    converted = dict(entry)
    for key in ("directory", "file", "output"):
        if isinstance(converted.get(key), str):
            converted[key] = rewriter.rewrite_path(converted[key])
    if "arguments" in converted:
        converted["arguments"] = rewriter.rewrite_arguments(converted["arguments"])
    elif "command" in converted:
        converted["command"] = shlex.join(
            rewriter.rewrite_arguments(shlex.split(converted["command"]))
        )
    return converted


VERIFY_TARGET_MARKER = "_verify_interface_header_sets.dir/"


def drop_verify_duplicates(entries):
    """Drop the entries of CMake's generated *_verify_interface_header_sets targets for a source
    that also has a real entry.

    Those scaffolding targets inherit INTERFACE_SOURCES (the conformance kit's .cpp files) and
    only the usage requirements of their library (cxx_std_17), whereas the real targets build
    the same files with their own flags. clang-tidy takes the first entry of a file, so it would
    judge the kit with the scaffolding's flags instead of the real build's.
    """
    real = {
        entry["file"]
        for entry in entries
        if VERIFY_TARGET_MARKER not in str(entry.get("output", ""))
    }
    return [
        entry
        for entry in entries
        if VERIFY_TARGET_MARKER not in str(entry.get("output", "")) or entry["file"] not in real
    ]


def convert(entries, container_root, host_root, output_dir):
    """Return (converted entries, warnings)."""
    warnings = []
    rewriter = Rewriter(container_root, host_root, output_dir / "rsp", warnings)
    converted = [convert_entry(entry, rewriter) for entry in entries]
    return drop_verify_duplicates(converted), warnings


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument(
        "--input",
        type=Path,
        default=REPO_ROOT / "build" / "dev" / "compile_commands.json",
        help="container compile database (default: build/dev/compile_commands.json)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=REPO_ROOT / "build" / "dev" / "host" / "compile_commands.json",
        help="host compile database (default: build/dev/host/compile_commands.json)",
    )
    parser.add_argument(
        "--container-root",
        default=DEFAULT_CONTAINER_ROOT,
        help="repository root inside the container (default: /src)",
    )
    parser.add_argument(
        "--host-root",
        type=Path,
        default=REPO_ROOT,
        help="repository root on the host (default: the root of this script's repository)",
    )
    return parser.parse_args(argv)


def run(args):
    if not args.input.is_file():
        raise InputMissing(
            f"{args.input} not found; configure first: scripts/dev cmake --preset dev"
        )
    entries = json.loads(args.input.read_text(encoding="utf-8"))
    output_dir = args.output.parent
    converted, warnings = convert(
        entries, args.container_root, str(args.host_root), output_dir
    )
    output_dir.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(converted, indent=2) + "\n", encoding="utf-8")
    return converted, warnings


def main(argv=None):
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        converted, warnings = run(args)
    except InputMissing as err:
        print(f"error: {err}", file=sys.stderr)
        return 1
    except (OSError, ValueError) as err:
        print(f"error: cannot convert {args.input}: {err}", file=sys.stderr)
        return 1
    for message in warnings:
        print(message, file=sys.stderr)
    print(f"wrote {len(converted)} entries to {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
