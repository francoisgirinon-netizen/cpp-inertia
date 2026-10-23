"""Tests for tools/coverage_check.py, run against small generated fixtures."""

import os
import subprocess
import sys
import tempfile
import tomllib
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SCRIPT = REPO / "tools" / "coverage_check.py"


def rule(rid, status="todo", reason=None, e2e=None):
    lines = [
        "[[rule]]",
        f'id = "{rid}"',
        f'behavior = "behavior of {rid}"',
        'source = "docs/inertia-protocol-v3.md#behaviors"',
        f'status = "{status}"',
    ]
    if reason is not None:
        lines.append(f'reason = "{reason}"')
    if e2e is not None:
        lines.append(f"e2e = {'true' if e2e else 'false'}")
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

    def e2e_list(self, *ids):
        path = self.dir / "e2e-list.txt"
        lines = ["Listing tests:"]
        for tid in ids:
            lines.append(f"  [client] > a.spec.ts:3:1 > does a thing [protocol:{tid}]")
        lines.append(f"Total: {len(ids)} tests in 1 file")
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")
        return path

    def run_check(self, matrix, tags_files=(), strict=False, e2e_list=None):
        cmd = [sys.executable, str(SCRIPT), "--matrix", str(matrix)]
        if e2e_list is not None:
            cmd += ["--e2e-list", str(e2e_list)]
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

    def test_e2e_missing_is_warning_unless_strict(self):
        matrix = self.matrix(rule("redirect.303", status="done", e2e=True))
        tags = self.tags("redirect.303")
        lenient = self.run_check(matrix, [tags], e2e_list=self.e2e_list())
        self.assertEqual(lenient.returncode, 0, lenient.stdout + lenient.stderr)
        self.assertIn("warning: missing e2e test for rule: redirect.303", lenient.stdout)

        strict = self.run_check(matrix, [tags], strict=True, e2e_list=self.e2e_list())
        self.assertEqual(strict.returncode, 1, strict.stdout + strict.stderr)
        self.assertIn("missing e2e test for rule: redirect.303", strict.stdout)

    def test_e2e_present_passes_strict(self):
        matrix = self.matrix(rule("redirect.303", status="done", e2e=True))
        result = self.run_check(
            matrix, [self.tags("redirect.303")], strict=True,
            e2e_list=self.e2e_list("redirect.303"),
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("missing e2e", result.stdout)

    def test_e2e_not_required_without_flag_or_list(self):
        matrix = self.matrix(rule("redirect.303", status="done"))
        tags = self.tags("redirect.303")
        with_list = self.run_check(matrix, [tags], strict=True, e2e_list=self.e2e_list())
        self.assertEqual(with_list.returncode, 0, with_list.stdout + with_list.stderr)

        flagged = self.matrix(rule("redirect.303", status="done", e2e=True))
        no_list = self.run_check(flagged, [tags], strict=True)
        self.assertEqual(no_list.returncode, 0, no_list.stdout + no_list.stderr)

    def test_e2e_unknown_tag_fails(self):
        matrix = self.matrix(rule("redirect.303", status="done"))
        result = self.run_check(
            matrix, [self.tags("redirect.303")], e2e_list=self.e2e_list("nope.x")
        )
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("unknown e2e protocol tag: nope.x", result.stdout)

    def test_e2e_must_be_boolean(self):
        matrix = self.dir / "bad.toml"
        matrix.write_text(rule("a.b", status="done").replace(
            'status = "done"', 'status = "done"\ne2e = "yes"'), encoding="utf-8")
        result = self.run_check(matrix, [self.tags("a.b")])
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("invalid e2e for rule a.b", result.stdout)

    def test_repository_matrix_is_consistent(self):
        matrix = REPO / "docs" / "protocol-coverage.toml"
        with open(matrix, "rb") as handle:
            ids = [r["id"] for r in tomllib.load(handle)["rule"]]
        result = self.run_check(matrix, [self.tags(*ids)])
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
