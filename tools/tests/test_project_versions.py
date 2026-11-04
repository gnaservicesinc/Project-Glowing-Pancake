#!/usr/bin/env python3
"""Identity contract tests; no release, game, or native-device qualification."""

from __future__ import annotations

from copy import deepcopy
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

TOOL = Path(__file__).resolve().parents[1] / "project_versions.py"
spec = importlib.util.spec_from_file_location("project_versions", TOOL)
versions = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = versions
spec.loader.exec_module(versions)
BASE = {
    "manifest_format": 1,
    "project_id": "project-glowing-pancake",
    "phase": "planning",
    "components": {"game": "0.0.0-dev", "editor": "0.0.0-dev", "content": "0.0.0-dev"},
    "compatibility": {"content_schema": 1, "simulation_rules": 1, "save_format": 1},
}


class VersionContractTests(unittest.TestCase):
    def test_checked_manifest_read_roundtrip(self):
        manifest = versions.load_manifest(versions.DEFAULT_MANIFEST)
        result = subprocess.run([sys.executable, str(TOOL), "read"], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout), manifest)

    def test_semver_precedence(self):
        ordered = ["1.0.0-alpha", "1.0.0-alpha.1", "1.0.0-alpha.beta", "1.0.0-beta",
                   "1.0.0-beta.2", "1.0.0-beta.11", "1.0.0-rc.1", "1.0.0", "1.0.1", "1.1.0", "2.0.0", "10.0.0"]
        for a, b in zip(ordered, ordered[1:]):
            with self.subTest(a=a, b=b):
                self.assertEqual(versions.compare_versions(a, b), -1)
                self.assertEqual(versions.compare_versions(b, a), 1)

    def test_build_metadata_has_no_precedence(self):
        self.assertEqual(versions.compare_versions("1.2.3+build.001", "1.2.3+other"), 0)
        self.assertEqual(versions.compare_versions("1.2.3-rc.1+a", "1.2.3-rc.1+b"), 0)

    def test_numeric_project_version_excludes_suffix(self):
        parsed = versions.parse_version("0.12.3-rc.10+source.42")
        self.assertEqual(parsed.numeric, "0.12.3")
        self.assertEqual(parsed.prerelease, ("rc", "10"))
        self.assertEqual(parsed.metadata, "source.42")

    def test_malformed_versions(self):
        invalid = ["1", "1.2", "v1.2.3", "01.2.3", "1.02.3", "1.2.03", "1.2.3-01", "1.2.3-a.01",
                   "1.2.3-", "1.2.3+", "1.2.3-a..b", "1.2.3+one+two", "1.2.3\n", "1.2.3-å", 1, True, None, "1" * 257]
        for value in invalid:
            with self.subTest(value=value), self.assertRaises(versions.VersionError):
                versions.parse_version(value)

    def test_non_numeric_prerelease_leading_zero_allowed(self):
        self.assertEqual(versions.parse_version("1.0.0-01a+0001").prerelease, ("01a",))

    def test_manifest_unknown_or_missing_fields(self):
        for path in ((), ("components",), ("compatibility",)):
            for operation in ("missing", "extra"):
                data = deepcopy(BASE)
                node = data
                for key in path:
                    node = node[key]
                if operation == "missing":
                    node.pop(next(iter(node)))
                else:
                    node["misspelled_field"] = 1
                with self.subTest(path=path, operation=operation), self.assertRaises(versions.VersionError):
                    versions.validate_manifest(data)

    def test_manifest_format_is_exact_integer(self):
        for value in (True, 1.0, "1", 2, 0):
            data = deepcopy(BASE)
            data["manifest_format"] = value
            with self.subTest(value=value), self.assertRaises(versions.VersionError):
                versions.validate_manifest(data)

    def test_compatibility_requires_bounded_integer(self):
        for key in BASE["compatibility"]:
            for value in (True, False, 1.0, "1", 0, -1, None, 2_147_483_648):
                data = deepcopy(BASE)
                data["compatibility"][key] = value
                with self.subTest(key=key, value=value), self.assertRaises(versions.VersionError):
                    versions.validate_manifest(data)

    def test_unknown_project_and_phase(self):
        for key, value in (("project_id", "other-game"), ("phase", "ready"), ("phase", [])):
            data = deepcopy(BASE)
            data[key] = value
            with self.subTest(key=key), self.assertRaises(versions.VersionError):
                versions.validate_manifest(data)

    def test_cmake_numeric_range(self):
        data = deepcopy(BASE)
        data["components"]["game"] = "2147483648.0.0"
        with self.assertRaises(versions.VersionError):
            versions.validate_manifest(data)

    def test_invalid_json_duplicate_fields_and_encoding(self):
        invalid = [b'{"manifest_format":1,"manifest_format":1}',
                   json.dumps(BASE).replace('"game": "0.0.0-dev"', '"game":"1.0.0","game":"2.0.0"').encode(),
                   b'{', b'\xff', b'NaN', b'9' * 5000,
                   b'[' * 2000 + b'0' + b']' * 2000, b' ' * 65_537]
        with tempfile.TemporaryDirectory() as scratch:
            path = Path(scratch) / "project-versions.json"
            for raw in invalid:
                path.write_bytes(raw)
                with self.subTest(raw=raw[:50]), self.assertRaises(versions.VersionError):
                    versions.load_manifest(path)

    def test_independent_product_tags_and_artifacts(self):
        data = deepcopy(BASE)
        data["components"]["game"] = "0.2.0-rc.1"
        data["components"]["editor"] = "0.7.0"
        self.assertEqual(versions.product_tag(data, "game"), "game-v0.2.0-rc.1")
        self.assertEqual(versions.product_tag(data, "editor"), "editor-v0.7.0")
        self.assertEqual(versions.artifact_stem(data, "editor", "linux", "arm64"), "pgp-editor-0.7.0-linux-arm64")
        with self.assertRaises(versions.VersionError):
            versions.product_tag(data, "content")
        with self.assertRaises(versions.VersionError):
            versions.artifact_stem(data, "game", "../../escape", "arm64")

    def test_planning_is_never_release_identity(self):
        errors = versions.identity_errors(BASE, "game", "game-v0.0.0-dev")
        self.assertTrue(any("phase" in item for item in errors))
        self.assertTrue(any("placeholder" in item for item in errors))

    def test_component_identity_gate_does_not_mix_products(self):
        data = deepcopy(BASE)
        data["phase"] = "release-candidate"
        data["components"].update(game="0.2.0-rc.1", editor="0.8.0-rc.1")
        self.assertEqual(versions.identity_errors(data, "editor", "editor-v0.8.0-rc.1", "0.7.0"), [])
        self.assertTrue(versions.identity_errors(data, "game", "editor-v0.8.0-rc.1"))

    def test_identity_rejects_nonmonotonic_and_metadata_only_reuse(self):
        data = deepcopy(BASE)
        data["phase"] = "release"
        data["components"]["game"] = "1.0.0+new"
        for previous in ("1.0.0+old", "1.0.1", "2.0.0"):
            with self.subTest(previous=previous):
                self.assertTrue(versions.identity_errors(data, "game", "game-v1.0.0+new", previous))
        self.assertEqual(versions.identity_errors(data, "game", "game-v1.0.0+new", "0.9.0"), [])

    def test_release_phase_disallows_product_prerelease(self):
        data = deepcopy(BASE)
        data["phase"] = "release"
        data["components"]["game"] = "1.0.0-rc.1"
        self.assertTrue(versions.identity_errors(data, "game", "game-v1.0.0-rc.1"))

    def test_export_source_is_unknown(self):
        with tempfile.TemporaryDirectory() as scratch:
            self.assertEqual(versions.source_identity(Path(scratch)), {"commit": "unavailable", "state": "unavailable"})

    @unittest.skipUnless(shutil.which("git"), "Git is unavailable")
    def test_source_records_commit_and_untracked_edits(self):
        with tempfile.TemporaryDirectory() as scratch:
            root = Path(scratch)
            def git(*args):
                return subprocess.run(["git", "-C", str(root), *args], check=True, capture_output=True, text=True).stdout.strip()
            git("init", "-q")
            git("-c", "user.name=Identity Fixture", "-c", "user.email=fixture@example.invalid", "commit", "--allow-empty", "-qm", "fixture")
            self.assertEqual(versions.source_identity(root), {"commit": git("rev-parse", "HEAD"), "state": "clean"})
            (root / "untracked.cpp").write_text("fixture")
            self.assertEqual(versions.source_identity(root)["state"], "modified")

    def test_release_cli_fails_closed_even_when_identity_matches(self):
        data = deepcopy(BASE)
        data["phase"] = "release"
        data["components"]["game"] = "1.0.0"
        with tempfile.TemporaryDirectory() as scratch:
            path = Path(scratch) / "project-versions.json"
            path.write_text(json.dumps(data))
            result = subprocess.run([sys.executable, str(TOOL), "--manifest", str(path),
                                     "check-release", "--product", "game", "--tag", "game-v1.0.0"], capture_output=True, text=True)
            self.assertEqual(result.returncode, 1)
            report = json.loads(result.stdout)
            self.assertFalse(report["release_ready"])
            self.assertTrue(report["remaining_gates"])

    @unittest.skipUnless(shutil.which("cmake"), "CMake is unavailable")
    def test_emitted_cmake_preserves_prerelease_and_numeric_core(self):
        data = deepcopy(BASE)
        data["components"].update(game="0.12.3-rc.10+build.4", editor="0.7.2")
        with tempfile.TemporaryDirectory() as scratch:
            root = Path(scratch)
            manifest = root / "project-versions.json"
            manifest.write_text(json.dumps(data))
            values = root / "values.cmake"
            values.write_text(versions.cmake_values(data, manifest))
            source = root / "CMakeLists.txt"
            source.write_text('cmake_minimum_required(VERSION 3.24)\n'
                              'include("${CMAKE_CURRENT_LIST_DIR}/values.cmake")\n'
                              'project(IdentityFixture VERSION "${PGP_VERSION_GAME_NUMERIC}" LANGUAGES NONE)\n'
                              'if(NOT PROJECT_VERSION STREQUAL "0.12.3" OR NOT PGP_VERSION_GAME STREQUAL "0.12.3-rc.10+build.4")\n'
                              'message(FATAL_ERROR "Numeric/full version identity differs")\nendif()\n')
            result = subprocess.run([shutil.which("cmake"), "-S", str(root), "-B", str(root / "build")], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
