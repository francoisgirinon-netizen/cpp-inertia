"""Tests for tools/check-inertia-version.py: the npm registry fetch is always mocked."""

import contextlib
import importlib.util
import io
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SCRIPT = REPO / "tools" / "check-inertia-version.py"

spec = importlib.util.spec_from_file_location("check_inertia_version", SCRIPT)
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


def registry_reply(version):
    return f'{{"name": "@inertiajs/react", "version": "{version}"}}'.encode()


class CheckInertiaVersionTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.matrix = Path(self._tmp.name) / "matrix.toml"
        self.matrix.write_text('inertia_react_version = "3.9.1"\n', encoding="utf-8")

    def tearDown(self):
        self._tmp.cleanup()

    def run_tool(self, fetch, matrix=None):
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            code = tool.main(["--matrix", str(matrix or self.matrix)], fetch=fetch)
        return code, out.getvalue()

    def test_equal_version_is_up_to_date(self):
        code, output = self.run_tool(lambda url: registry_reply("3.9.1"))
        self.assertEqual(code, 0)
        self.assertIn("up to date", output)

    def test_newer_version_exits_1_with_the_message(self):
        code, output = self.run_tool(lambda url: registry_reply("3.10.0"))
        self.assertEqual(code, 1)
        self.assertIn("new @inertiajs/react 3.10.0 available (matrix pinned to 3.9.1)", output)

    def test_queries_the_latest_endpoint_of_the_registry(self):
        seen = []

        def fetch(url):
            seen.append(url)
            return registry_reply("3.9.1")

        self.run_tool(fetch)
        self.assertEqual(seen, ["https://registry.npmjs.org/@inertiajs/react/latest"])

    def test_network_failure_exits_2(self):
        def fetch(url):
            raise OSError("no network")

        code, output = self.run_tool(fetch)
        self.assertEqual(code, 2)
        self.assertIn("cannot fetch", output)

    def test_malformed_reply_exits_2(self):
        code, output = self.run_tool(lambda url: b"<html>not json</html>")
        self.assertEqual(code, 2)
        self.assertIn("unexpected registry reply", output)

    def test_reply_without_version_exits_2(self):
        code, _ = self.run_tool(lambda url: b'{"name": "x"}')
        self.assertEqual(code, 2)

    def test_matrix_without_pinned_version_exits_2(self):
        self.matrix.write_text("[[rule]]\n", encoding="utf-8")
        code, output = self.run_tool(lambda url: registry_reply("3.9.1"))
        self.assertEqual(code, 2)
        self.assertIn("inertia_react_version", output)

    def test_unreadable_matrix_exits_2(self):
        code, _ = self.run_tool(
            lambda url: registry_reply("3.9.1"), matrix=self.matrix.with_name("none.toml")
        )
        self.assertEqual(code, 2)

    def test_repository_matrix_pins_a_version(self):
        pinned = tool.pinned_version(REPO / "docs" / "protocol-coverage.toml")
        self.assertRegex(pinned, r"^\d+\.\d+\.\d+$")


if __name__ == "__main__":
    unittest.main()
