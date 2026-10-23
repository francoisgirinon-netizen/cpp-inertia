#!/usr/bin/env python3
"""Check that protocol coverage rules and test tags agree.

Every `done` rule in the matrix needs a test tagged `[protocol:<id>]`, and every
tag found in a test executable must name a rule in the matrix.
"""

import argparse
import re
import subprocess
import sys
import tomllib

TAG_RE = re.compile(r"\[protocol:([a-z0-9.\-]+)\]")
STATUSES = ("todo", "done", "na")
REQUIRED_FIELDS = ("id", "behavior", "source", "status")


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--matrix", required=True, help="path to protocol-coverage.toml")
    parser.add_argument(
        "--tags-from",
        action="append",
        default=[],
        metavar="EXE",
        help="test executable whose --list-tags output is scanned (repeatable)",
    )
    parser.add_argument(
        "--tags-file",
        action="append",
        default=[],
        metavar="PATH",
        help="file holding --list-tags style output (repeatable; test hook)",
    )
    parser.add_argument(
        "--strict", action="store_true", help="fail on rules still marked todo"
    )
    return parser.parse_args(argv)


def tags_from_text(text):
    return set(TAG_RE.findall(text))


def tags_from_exe(exe, problems):
    try:
        result = subprocess.run(
            [exe, "--list-tags"], capture_output=True, text=True, check=False
        )
    except OSError as err:
        problems.append(f"cannot run {exe} --list-tags: {err}")
        return set()
    if result.returncode != 0:
        problems.append(f"{exe} --list-tags exited with {result.returncode}")
        return set()
    return tags_from_text(result.stdout)


def check(rules, tags, strict, problems):
    """Append a message to `problems` for every violation; return counts."""
    ids = set()
    counts = {status: 0 for status in STATUSES}
    for rule in rules:
        rid = rule.get("id", "<no id>")
        for field in REQUIRED_FIELDS:
            if not str(rule.get(field, "")).strip():
                problems.append(f"missing field {field} in rule: {rid}")
        if rid in ids:
            problems.append(f"duplicate rule: {rid}")
        ids.add(rid)

        status = rule.get("status")
        if status not in STATUSES:
            problems.append(f"invalid status for rule {rid}: {status}")
            continue
        counts[status] += 1
        if status == "na" and not str(rule.get("reason", "")).strip():
            problems.append(f"missing reason for na rule: {rid}")
        if status == "done" and rid not in tags:
            problems.append(f"missing test for done rule: {rid}")
        if status == "todo" and strict:
            problems.append(f"todo rule in strict mode: {rid}")

    for tag in sorted(tags - ids):
        problems.append(f"unknown protocol tag: {tag}")
    return len(ids), counts


def main(argv):
    args = parse_args(argv)
    problems = []
    try:
        with open(args.matrix, "rb") as handle:
            rules = tomllib.load(handle).get("rule", [])
    except (OSError, tomllib.TOMLDecodeError) as err:
        print(f"cannot read matrix {args.matrix}: {err}")
        return 1

    tags = set()
    for path in args.tags_file:
        with open(path, encoding="utf-8") as handle:
            tags |= tags_from_text(handle.read())
    for exe in args.tags_from:
        tags |= tags_from_exe(exe, problems)

    total, counts = check(rules, tags, args.strict, problems)
    for problem in problems:
        print(problem)
    if problems:
        print(f"protocol coverage check failed: {len(problems)} problem(s)")
        return 1
    print(
        f"protocol coverage ok: {total} rules "
        f"({counts['done']} done, {counts['todo']} todo, {counts['na']} na)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
