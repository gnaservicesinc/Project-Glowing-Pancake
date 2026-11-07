"""Exercise wrapper deletion boundaries and command failures without building SDKs.

Run explicitly: python3 tools/tests/test_build_wrapper.py
The build itself does not depend on Python.
"""

from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest


SOURCE_ROOT = Path(__file__).resolve().parents[2]


class BuildWrapperTests(unittest.TestCase):
    def setUp(self):
        scratch = SOURCE_ROOT / ".work"
        scratch.mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="wrapper-tests-", dir=scratch)
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "checkout with spaces"
        (self.root / "tools").mkdir(parents=True)
        for relative in ("make.sh", "tools/build.sh"):
            shutil.copy2(SOURCE_ROOT / relative, self.root / relative)
        self.bin = Path(self.temporary.name) / "fake-bin"
        self.bin.mkdir()
        self.log = Path(self.temporary.name) / "commands"
        self.env = os.environ.copy()
        for key in ("CC", "CXX", "QT_ROOT", "CMAKE_PREFIX_PATH", "DEVELOPER_DIR"):
            self.env.pop(key, None)
        self.env.update(PATH=f"{self.bin}{os.pathsep}{self.env['PATH']}", PGP_TEST_LOG=str(self.log))
        self.write_tool("uname", '#!/bin/sh\nprintf "Linux\\n"\n')
        self.write_tool("ninja", '#!/bin/sh\nexit 0\n')
        self.write_tool("cmake", '''#!/bin/sh
printf 'cmake %s\\n' "$*" >> "$PGP_TEST_LOG"
if [ "$1" = --build ]; then exit "${PGP_TEST_BUILD_STATUS:-0}"; fi
exit "${PGP_TEST_CONFIGURE_STATUS:-0}"
''')
        self.write_tool("ctest", '''#!/bin/sh
printf 'ctest %s\\n' "$*" >> "$PGP_TEST_LOG"
exit "${PGP_TEST_CTEST_STATUS:-0}"
''')

    def write_tool(self, name, text):
        path = self.bin / name
        path.write_text(text)
        path.chmod(0o755)

    def run_build(self, *arguments, **environment):
        return subprocess.run(
            ["bash", str(self.root / "make.sh"), *arguments],
            cwd=self.temporary.name,
            env={**self.env, **environment},
            text=True,
            capture_output=True,
        )

    def commands(self):
        return self.log.read_text() if self.log.exists() else ""

    def test_default_after_debug_uses_release(self):
        self.assertEqual(self.run_build("--debug").returncode, 0)
        result = self.run_build()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("cmake --preset debug", self.commands())
        self.assertIn("cmake --preset release", self.commands())
        self.assertTrue((self.root / "build/debug/.pgp-build-owner").is_file())
        self.assertTrue((self.root / "build/release/.pgp-build-owner").is_file())

    def test_core_build_ignores_qt_root(self):
        result = self.run_build("--core-only", "--qt-root", "/nonexistent-qt")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("cmake --preset core-release", self.commands())
        self.assertNotIn("CMAKE_PREFIX_PATH", self.commands())

    def test_build_failure_stops_before_tests(self):
        result = self.run_build("--core-only", "--test", PGP_TEST_BUILD_STATUS="17")
        self.assertEqual(result.returncode, 17)
        self.assertNotIn("ctest", self.commands())

    def test_configure_failure_stops_before_build(self):
        result = self.run_build("--core-only", "--test", PGP_TEST_CONFIGURE_STATUS="19")
        self.assertEqual(result.returncode, 19)
        self.assertNotIn("--build", self.commands())

    def test_ctest_failure_reaches_caller(self):
        result = self.run_build("--core-only", "--test", PGP_TEST_CTEST_STATUS="23")
        self.assertEqual(result.returncode, 23)
        self.assertIn("ctest --preset core-release", self.commands())
        self.assertNotIn("Built core-release", result.stdout)

    def test_clean_removes_only_selected_owned_tree(self):
        self.assertEqual(self.run_build("--core-only").returncode, 0)
        self.assertEqual(self.run_build("--core-only", "--debug").returncode, 0)
        foreign = self.root / "build/other-owner"
        foreign.mkdir()
        (foreign / "keep").write_text("user data")
        before = self.commands()
        result = self.run_build("--core-only", "--clean")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse((self.root / "build/core-release").exists())
        self.assertTrue((self.root / "build/core-debug").is_dir())
        self.assertEqual((foreign / "keep").read_text(), "user data")
        self.assertEqual(self.commands(), before)

    def test_clean_rejects_unowned_tree(self):
        foreign = self.root / "build/core-release"
        foreign.mkdir(parents=True)
        (foreign / "keep").write_text("user data")
        self.assertNotEqual(self.run_build("--core-only", "--clean").returncode, 0)
        self.assertEqual((foreign / "keep").read_text(), "user data")

    def test_clean_rejects_other_checkouts_owner_marker(self):
        foreign = self.root / "build/core-release"
        foreign.mkdir(parents=True)
        (foreign / ".pgp-build-owner").write_text("/another/checkout\n")
        (foreign / "keep").write_text("user data")
        self.assertNotEqual(self.run_build("--core-only", "--clean").returncode, 0)
        self.assertTrue((foreign / "keep").exists())

    def test_symlink_parent_and_tree_are_rejected(self):
        external = Path(self.temporary.name) / "external"
        external.mkdir()
        (external / "keep").write_text("user data")
        (self.root / "build").symlink_to(external, target_is_directory=True)
        for flags in (("--core-only",), ("--core-only", "--clean")):
            self.assertNotEqual(self.run_build(*flags).returncode, 0)
            self.assertEqual((external / "keep").read_text(), "user data")
        (self.root / "build").unlink()
        (self.root / "build").mkdir()
        (self.root / "build/core-release").symlink_to(external, target_is_directory=True)
        self.assertNotEqual(self.run_build("--core-only", "--clean").returncode, 0)
        self.assertEqual((external / "keep").read_text(), "user data")

    def test_changed_toolchain_identity_is_rejected(self):
        self.assertEqual(self.run_build("--core-only", CXX="first-cxx").returncode, 0)
        before = self.commands()
        result = self.run_build("--core-only", CXX="different-cxx")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("cached toolchain/configuration changed", result.stderr)
        self.assertEqual(before, self.commands())

    def test_reserved_unknown_and_invalid_flags_have_no_side_effects(self):
        for flags in (("--all",), ("--package",), ("--unknown",), ("--editor", "--core-only"),
                      ("--editor", "--clean"), ("--jobs", "0"), ("--qt-root",), ("--clean", "--test")):
            with self.subTest(flags=flags):
                self.assertNotEqual(self.run_build(*flags).returncode, 0)
                self.assertFalse((self.root / "build").exists())
                self.assertEqual(self.commands(), "")

    def test_editor_selection_and_explicit_tests(self):
        self.assertEqual(self.run_build("--editor", "--test").returncode, 0)
        self.assertIn("cmake --preset release -DPGP_BUILD_EDITOR=ON", self.commands())
        self.assertIn("-DBUILD_TESTING=ON", self.commands())
        self.assertIn("ctest --preset release -R ^studio[.]", self.commands())
        self.assertEqual(self.run_build().returncode, 0)
        self.assertIn("cmake --preset release -DPGP_BUILD_EDITOR=ON -DBUILD_TESTING=OFF", self.commands())

    def test_default_builds_both_apps_without_tests_or_debugging(self):
        result=self.run_build()
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn("cmake --preset release -DPGP_BUILD_EDITOR=ON -DBUILD_TESTING=OFF -DPGP_NATIVE_OPTIMIZATION=ON",self.commands())
        self.assertIn("cmake --build --preset release --target pgp-game pgp-editor --parallel 4",self.commands())
        self.assertNotIn("ctest",self.commands())
        self.assertNotIn("--preset debug",self.commands())

    def test_testing_is_opt_in_and_disabled_again_on_normal_build(self):
        self.assertEqual(self.run_build("--enable-tests").returncode,0)
        self.assertIn("-DBUILD_TESTING=ON",self.commands())
        self.assertNotIn("ctest",self.commands())
        self.assertEqual(self.run_build().returncode,0)
        self.assertIn("-DBUILD_TESTING=OFF",self.commands())

    def test_debugging_alias_and_portable_build(self):
        result=self.run_build("--enable-debugging","--portable")
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn("cmake --preset debug",self.commands())
        self.assertIn("-DPGP_NATIVE_OPTIMIZATION=OFF",self.commands())
        self.assertIn("-DBUILD_TESTING=OFF",self.commands())


if __name__ == "__main__":
    unittest.main(verbosity=2)
