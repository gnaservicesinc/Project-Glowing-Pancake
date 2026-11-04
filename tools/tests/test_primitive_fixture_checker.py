#!/usr/bin/env python3
"""Exercise the parity checker's rejection and process-result boundaries."""

from contextlib import redirect_stderr, redirect_stdout
import importlib.util
from io import StringIO
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch


REPOSITORY = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "primitive_fixture_checker", REPOSITORY / "tools/check_primitive_fixture.py"
)
checker = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(checker)


class PrimitiveFixtureCheckerTests(unittest.TestCase):
    def test_accepts_full_width_signed_and_unsigned_integers(self):
        checker.validate_json(b'{"signed":-9223372036854775808,"unsigned":18446744073709551615}\n')

    def test_rejects_float_nonfinite_duplicate_and_nonascii_json(self):
        for data in (
            b'{"value":1.0}', b'{"value":1e2}', b'{"value":NaN}',
            b'{"value":Infinity}', b'{"value":1,"value":2}',
            b'{"nested":{"value":1,"value":2}}', b'{"value":"\xff"}',
        ):
            with self.subTest(data=data), self.assertRaises(ValueError):
                checker.validate_json(data)

    def invoke(self, actual, expected=b'{"value":1}\n', returncode=0, stderr=b''):
        scratch = REPOSITORY / ".work"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="fixture-checker-test-", dir=scratch) as directory:
            root = Path(directory)
            executable = root / "mock-executable"
            executable.touch()
            golden = root / "expected.json"
            golden.write_bytes(expected)
            output = root / "observed.json"
            arguments = [
                "check_primitive_fixture.py", "--executable", str(executable),
                "--golden", str(golden), "--output", str(output),
            ]
            process = subprocess.CompletedProcess([str(executable)], returncode, actual, stderr)
            stdout_capture, stderr_capture = StringIO(), StringIO()
            with patch.object(sys, "argv", arguments), patch.object(
                checker.subprocess, "run", return_value=process
            ) as run, redirect_stdout(stdout_capture), redirect_stderr(stderr_capture):
                status = checker.main()
            run.assert_called_once_with(
                [str(executable.resolve())], capture_output=True, timeout=60, check=False
            )
            observed = output.read_bytes() if output.exists() else None
            return status, stdout_capture.getvalue(), stderr_capture.getvalue(), observed

    def test_normalizes_windows_newlines_and_emits_matching_evidence(self):
        status, stdout, stderr, observed = self.invoke(b'{"value":1}\r\n')
        self.assertEqual(status, 0)
        self.assertIn("SHA-256", stdout)
        self.assertEqual(stderr, "")
        self.assertEqual(observed, b'{"value":1}\n')

    def test_integer_mismatch_fails_and_preserves_observed_output(self):
        status, stdout, stderr, observed = self.invoke(b'{"value":2}\n')
        self.assertEqual(status, 1)
        self.assertEqual(stdout, "")
        self.assertIn("differs at byte", stderr)
        self.assertEqual(observed, b'{"value":2}\n')

    def test_float_output_is_rejected_before_evidence(self):
        status, stdout, stderr, observed = self.invoke(b'{"value":1.0}\n')
        self.assertEqual(status, 1)
        self.assertIn("non-integer JSON number", stderr)
        self.assertIsNone(observed)

    def test_failed_process_cannot_pass_by_printing_expected_json(self):
        status, stdout, stderr, observed = self.invoke(b'{"value":1}\n', returncode=2, stderr=b"bad state\n")
        self.assertEqual(status, 1)
        self.assertIn("exited 2", stderr)
        self.assertIn("bad state", stderr)
        self.assertIsNone(observed)


if __name__ == "__main__":
    unittest.main()
