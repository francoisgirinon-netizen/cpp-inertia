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
        "--e2e-list",
        metavar="PATH",
        help="output of `playwright test --list`; rules with e2e = true need a "
        "[protocol:<id>] test there (warning, error with --strict)",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="fail on rules still marked todo and on client-observable rules without an e2e test",
    )
    parser.add_argument(
        "--markdown",
        action="store_true",
        help="print the matrix as a Markdown table (used for the README) and exit",
    )
    return parser.parse_args(argv)


def markdown_cell(text):
    return " ".join(str(text).split()).replace("|", "\\|")


def markdown_table(rules):
    """The matrix as a Markdown table, one row per rule, in matrix order."""
    lines = [
        "| Rule | Behavior | Status | Browser-tested |",
        "| --- | --- | --- | --- |",
    ]
    for rule in rules:
        status = str(rule.get("status", ""))
        if status == "na":
            status = f"n/a: {markdown_cell(rule.get('reason', ''))}"
        browser = "yes" if rule.get("e2e") is True else "no"
        lines.append(
            f"| `{markdown_cell(rule.get('id', ''))}` | {markdown_cell(rule.get('behavior', ''))} "
            f"| {status} | {browser} |"
        )
    return "\n".join(lines)


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


def matrix_problems(data, rules):
    """Problems with the matrix header and rule list, independent of any test tags."""
    problems = []
    pinned = data.get("inertia_react_version")
    if not isinstance(pinned, str) or not pinned.strip():
        problems.append("missing or non-string inertia_react_version in matrix")
    if not rules:
        problems.append("no [[rule]] entries in matrix")
    return problems


def check(rules, tags, strict, problems, e2e_tags=None, warnings=None):
    """Append a message to `problems` for every violation; return counts.

    With `e2e_tags` (the ids tagged in the Playwright list), every rule with `e2e = true`
    must be among them: a warning, or a problem in strict mode.
    """
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
        e2e = rule.get("e2e", False)
        if not isinstance(e2e, bool):
            problems.append(f"invalid e2e for rule {rid}: {e2e!r} (expected true or false)")
        elif e2e and e2e_tags is not None and rid not in e2e_tags:
            message = f"missing e2e test for rule: {rid}"
            if strict:
                problems.append(message)
            elif warnings is not None:
                warnings.append(f"warning: {message}")
        if status == "todo" and strict:
            problems.append(f"todo rule in strict mode: {rid}")

    for tag in sorted(tags - ids):
        problems.append(f"unknown protocol tag: {tag}")
    for tag in sorted((e2e_tags or set()) - ids):
        problems.append(f"unknown e2e protocol tag: {tag}")
    return len(ids), counts


def main(argv):
    args = parse_args(argv)
    problems = []
    try:
        with open(args.matrix, "rb") as handle:
            data = tomllib.load(handle)
    except (OSError, tomllib.TOMLDecodeError) as err:
        print(f"cannot read matrix {args.matrix}: {err}")
        return 1

    rules = data.get("rule", [])
    header_problems = matrix_problems(data, rules)
    if header_problems:
        for problem in header_problems:
            print(problem)
        print(f"protocol coverage check failed: {len(header_problems)} problem(s)")
        return 1

    if args.markdown:
        print(markdown_table(rules))
        return 0

    tags = set()
    for path in args.tags_file:
        try:
            with open(path, encoding="utf-8") as handle:
                tags |= tags_from_text(handle.read())
        except (OSError, UnicodeDecodeError) as err:
            print(f"cannot read tags file {path}: {err}")
            return 1
    for exe in args.tags_from:
        tags |= tags_from_exe(exe, problems)

    e2e_tags = None
    if args.e2e_list:
        try:
            with open(args.e2e_list, encoding="utf-8") as handle:
                e2e_tags = tags_from_text(handle.read())
        except OSError as err:
            print(f"cannot read e2e list {args.e2e_list}: {err}")
            return 1

    warnings = []
    total, counts = check(rules, tags, args.strict, problems, e2e_tags, warnings)
    for warning in warnings:
        print(warning)
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
