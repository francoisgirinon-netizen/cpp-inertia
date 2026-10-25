"""Tests for tools/host-compile-commands.py: fixture database, temporary host root, no Docker."""

import contextlib
import importlib.util
import io
import json
import shlex
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SCRIPT = REPO / "tools" / "host-compile-commands.py"
FIXTURE = Path(__file__).resolve().parent / "fixtures" / "compile_commands.json"

spec = importlib.util.spec_from_file_location("host_compile_commands", SCRIPT)
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)

HOST = "/home/dev-host/cpp-inertia"


def tokens(entry):
    if "arguments" in entry:
        return list(entry["arguments"])
    return shlex.split(entry["command"])


class ConvertTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        self.host_root = self.root / "host"
        self.output = self.root / "out" / "compile_commands.json"
        self.entries = json.loads(FIXTURE.read_text(encoding="utf-8"))

    def tearDown(self):
        self._tmp.cleanup()

    def convert(self, entries=None, host_root=None):
        output_dir = self.output.parent
        converted, warnings = tool.convert(
            self.entries if entries is None else entries,
            "/src",
            str(host_root or HOST),
            output_dir,
        )
        return converted, warnings

    def test_rewrites_directory_file_and_output(self):
        converted, _ = self.convert()
        first = converted[0]
        self.assertEqual(first["directory"], f"{HOST}/build/dev")
        self.assertEqual(first["file"], f"{HOST}/src/inertia.cpp")
        self.assertEqual(first["output"], f"{HOST}/build/dev/CMakeFiles/inertia_core.dir/src/inertia.cpp.o")

    def test_rewrites_joined_and_separate_include_flags(self):
        converted, _ = self.convert()
        args = tokens(converted[0])
        self.assertIn("-I" + HOST + "/include", args)
        self.assertIn(HOST + "/build/dev/vcpkg_installed/x64-linux/include", args)

        args = tokens(converted[1])
        self.assertEqual(args[args.index("-I") + 1], HOST + "/adapters/drogon/include")
        self.assertEqual(args[args.index("-iquote") + 1], HOST + "/tests/drogon")
        self.assertEqual(args[args.index("-include") + 1], HOST + "/include/inertia/prelude.hpp")
        self.assertIn("--include=" + HOST + "/include/inertia/extra.hpp", args)
        self.assertIn("-include" + HOST + "/include/inertia/joined.hpp", args)

    def test_rewrites_source_file_argument(self):
        converted, _ = self.convert()
        args = tokens(converted[1])
        self.assertEqual(args[-1], HOST + "/tests/drogon/main.cpp")

    def test_leaves_other_paths_untouched(self):
        converted, _ = self.convert()
        args0 = tokens(converted[0])
        self.assertIn("-isystem", args0)
        self.assertIn("/usr/include", args0)
        self.assertIn("CMakeFiles/inertia_core.dir/src/inertia.cpp.o", args0)

        args1 = tokens(converted[1])
        # '/srcfoo' is a sibling of the container root, not below it
        self.assertIn("-I/srcfoo/include", args1)
        self.assertIn("-I/usr/include/c++/15", args1)
        self.assertEqual(converted[1]["output"], "main.o")

    def test_drops_gcc_install_dir_and_sysroot(self):
        converted, _ = self.convert()
        args = tokens(converted[1])
        self.assertFalse(any(arg.startswith("--gcc-install-dir") for arg in args))
        self.assertNotIn("--sysroot", args)
        self.assertNotIn("/", args)
        # the flags around them survive
        self.assertIn("-c", args)

    def test_command_string_form_is_kept_as_string(self):
        converted, _ = self.convert()
        self.assertIn("command", converted[2])
        self.assertNotIn("arguments", converted[2])
        self.assertTrue(converted[2]["command"].startswith("/usr/local/bin/clang++ @"))

    def test_response_file_is_rewritten_and_its_content_too(self):
        rsp_host = self.host_root / "build" / "dev" / "rsp" / "args.rsp"
        rsp_host.parent.mkdir(parents=True)
        rsp_host.write_text("-I/src/include -DNAME='a b' -std=c++20\n", encoding="utf-8")

        converted, warnings = self.convert(host_root=self.host_root)
        self.assertEqual(warnings, [])
        at = [arg for arg in tokens(converted[2]) if arg.startswith("@")][0]
        rsp_out = Path(at[1:])
        self.assertTrue(rsp_out.is_file())
        self.assertEqual(rsp_out.parent, self.output.parent / "rsp")
        rewritten = shlex.split(rsp_out.read_text(encoding="utf-8"))
        self.assertEqual(rewritten[0], "-I" + str(self.host_root) + "/include")
        self.assertIn("-DNAME=a b", rewritten)
        self.assertIn("-std=c++20", rewritten)

    def test_verify_target_entries_are_dropped_when_a_real_entry_exists(self):
        def entry(source, output):
            return {
                "directory": "/src/build/dev",
                "file": f"/src/{source}",
                "output": output,
                "command": f"clang++ -c /src/{source}",
            }

        verify = "conformance/CMakeFiles/inertia_conformance_verify_interface_header_sets.dir/scenarios.cpp.o"
        real = "tests/conformance/CMakeFiles/inertia_conformance_fake.dir/__/__/conformance/scenarios.cpp.o"
        only_verify = "conformance/CMakeFiles/x_verify_interface_header_sets.dir/lonely.cpp.o"
        converted, _ = self.convert(
            [
                entry("conformance/scenarios.cpp", verify),
                entry("conformance/scenarios.cpp", real),
                entry("conformance/lonely.cpp", only_verify),
            ]
        )
        self.assertEqual(
            [e["output"].replace(HOST + "/", "") for e in converted],
            [real, only_verify],
        )

    def test_missing_response_file_is_reported_and_path_rewritten(self):
        converted, warnings = self.convert(host_root=self.host_root)
        at = [arg for arg in tokens(converted[2]) if arg.startswith("@")][0]
        self.assertEqual(at, "@" + self.host_root.as_posix() + "/build/dev/rsp/args.rsp")
        self.assertEqual(len(warnings), 1)
        self.assertIn("args.rsp", warnings[0])

    def test_container_root_is_configurable(self):
        entries = [{"directory": "/work/build", "command": "clang++ -I/work/include -c /work/a.cpp",
                    "file": "/work/a.cpp", "output": "a.o"}]
        converted, _ = tool.convert(entries, "/work", "/h", self.output.parent)
        self.assertEqual(converted[0]["file"], "/h/a.cpp")
        self.assertEqual(tokens(converted[0])[1], "-I/h/include")

    def test_main_writes_output_and_reports_count(self):
        out = io.StringIO()
        err = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = tool.main([
                "--input", str(FIXTURE),
                "--output", str(self.output),
                "--host-root", str(self.host_root),
            ])
        self.assertEqual(code, 0)
        written = json.loads(self.output.read_text(encoding="utf-8"))
        self.assertEqual(len(written), len(self.entries))
        self.assertIn("3 entries", out.getvalue())
        self.assertIn("response file", err.getvalue())

    def test_missing_input_exits_nonzero_with_hint(self):
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            code = tool.main([
                "--input", str(self.root / "absent" / "compile_commands.json"),
                "--output", str(self.output),
            ])
        self.assertNotEqual(code, 0)
        self.assertIn("configure first: scripts/dev cmake --preset dev", err.getvalue())
        self.assertFalse(self.output.exists())

    def test_default_paths_are_under_the_repository(self):
        args = tool.parse_args([])
        self.assertEqual(args.input, REPO / "build" / "dev" / "compile_commands.json")
        self.assertEqual(args.output, REPO / "build" / "dev" / "host" / "compile_commands.json")
        self.assertEqual(args.host_root, REPO)
        self.assertEqual(args.container_root, "/src")


if __name__ == "__main__":
    unittest.main()
