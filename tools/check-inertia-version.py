#!/usr/bin/env python3
"""Compare the newest @inertiajs/react on npm with the version the coverage matrix is pinned to.

Exit codes: 0 up to date, 1 a different (newer) version is published, 2 the check itself failed
(registry unreachable or unexpected reply, unreadable matrix).
"""

import argparse
import json
import sys
import tomllib
import urllib.request
from pathlib import Path

REGISTRY_URL = "https://registry.npmjs.org/@inertiajs/react/latest"
DEFAULT_MATRIX = Path(__file__).resolve().parents[1] / "docs" / "protocol-coverage.toml"
TIMEOUT_SECONDS = 15


class CheckError(Exception):
    pass


def fetch_url(url):
    request = urllib.request.Request(url, headers={"Accept": "application/json"})
    with urllib.request.urlopen(request, timeout=TIMEOUT_SECONDS) as response:  # noqa: S310
        return response.read()


def pinned_version(matrix):
    try:
        with open(matrix, "rb") as handle:
            version = tomllib.load(handle).get("inertia_react_version")
    except (OSError, tomllib.TOMLDecodeError) as err:
        raise CheckError(f"cannot read matrix {matrix}: {err}") from err
    if not isinstance(version, str) or not version:
        raise CheckError(f"no inertia_react_version in {matrix}")
    return version


def latest_version(fetch):
    try:
        raw = fetch(REGISTRY_URL)
    except (OSError, ValueError) as err:  # URLError and timeouts are OSErrors
        raise CheckError(f"cannot fetch {REGISTRY_URL}: {err}") from err
    try:
        version = json.loads(raw).get("version")
    except (ValueError, AttributeError) as err:
        raise CheckError(f"unexpected registry reply from {REGISTRY_URL}") from err
    if not isinstance(version, str) or not version:
        raise CheckError(f"unexpected registry reply from {REGISTRY_URL}: no version")
    return version


def main(argv, fetch=fetch_url):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--matrix", default=str(DEFAULT_MATRIX), help="path to protocol-coverage.toml")
    args = parser.parse_args(argv)
    try:
        pinned = pinned_version(args.matrix)
        latest = latest_version(fetch)
    except CheckError as err:
        print(err, file=sys.stderr)
        return 2
    if latest == pinned:
        print(f"@inertiajs/react {pinned} is up to date")
        return 0
    print(f"new @inertiajs/react {latest} available (matrix pinned to {pinned})")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
