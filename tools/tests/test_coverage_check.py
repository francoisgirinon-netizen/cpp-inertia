"""Tests for tools/coverage_check.py, run against small generated fixtures."""

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SCRIPT = REPO / "tools" / "coverage_check.py"


def rule(rid, status="todo", reason=None):
    lines = [
        "[[rule]]",
        f'id = "{rid}"',
        f'behavior = "behavior of {rid}"',
        f'source = "docs/inertia-protocol-v3.md#behaviors"',
        f'status = "{status}"',
    ]
    if reason is not None:
        lines.append(f'reason = "{reason}"')
    return "\n".join(lines) + "\n"


def list_tags_output(*ids):
    lines = ["All available tags:"]
    for i, tid in enumerate(ids, 1):
        lines.append(f"  {i}  [protocol:{tid}]")
    return "\n".join(lines) + "\n"


class CoverageCheckTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self._tmp.name)

    def tearDown(self):
        self._tmp.cleanup()

    def write(self, name, text):
        path = self.dir / name
        path.write_text(text, encoding="utf-8")
        return path

    def matrix(self, *rules):
        body = 'inertia_react_version = "3.9.1"\n\n' + "\n".join(rules)
        return self.write("matrix.toml", body)

    def tags(self, *ids):
        return self.write("tags.txt", list_tags_output(*ids))

    def run_check(self, matrix, tags_files=(), strict=False):
        cmd = [sys.executable, str(SCRIPT), "--matrix", str(matrix)]
        for tags in tags_files:
            cmd += ["--tags-file", str(tags)]
        if strict:
            cmd.append("--strict")
        return subprocess.run(cmd, capture_output=True, text=True, check=False)

    def test_done_without_tag_fails(self):
        matrix = self.matrix(rule("partial.except", status="done"))
        result = self.run_check(matrix, [self.tags()])
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("missing test for done rule: partial.except", result.stdout)

    def test_unknown_tag_fails(self):
        matrix = self.matrix(rule("partial.except", status="todo"))
        result = self.run_check(matrix, [self.tags("foo.bar")])
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("unknown protocol tag: foo.bar", result.stdout)

    def test_todo_ok_unless_strict(self):
        matrix = self.matrix(rule("x.y", status="todo"))
        tags = self.tags()
        lenient = self.run_check(matrix, [tags])
        self.assertEqual(lenient.returncode, 0, lenient.stdout + lenient.stderr)

        strict = self.run_check(matrix, [tags], strict=True)
        self.assertEqual(strict.returncode, 1, strict.stdout + strict.stderr)
        self.assertIn("todo rule in strict mode: x.y", strict.stdout)

    def test_na_requires_reason(self):
        matrix = self.matrix(rule("precognition", status="na"))
        result = self.run_check(matrix, [self.tags()])
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("missing reason for na rule: precognition", result.stdout)

    def test_done_with_tag_passes(self):
        matrix = self.matrix(rule("partial.except", status="done"))
        result = self.run_check(matrix, [self.tags("partial.except")])
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_tags_are_read_from_list_tags_executable(self):
        exe = self.dir / "fake_tests"
        exe.write_text(
            f"#!{sys.executable}\n"
            "import sys\n"
            f"sys.stdout.write({list_tags_output('partial.except')!r})\n",
            encoding="utf-8",
        )
        os.chmod(exe, 0o755)
        matrix = self.matrix(rule("partial.except", status="done"))
        cmd = [sys.executable, str(SCRIPT), "--matrix", str(matrix),
               "--tags-from", str(exe)]
        result = subprocess.run(cmd, capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_repository_matrix_is_consistent(self):
        matrix = REPO / "docs" / "protocol-coverage.toml"
        result = self.run_check(matrix, [self.tags()])
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
