#!/usr/bin/env python3
"""Compare the native primitive fixture to its independent integer-only oracle."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def reject_noninteger(value: str) -> None:
    raise ValueError(f"non-integer JSON number: {value}")


def unique_object(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def validate_json(data: bytes) -> None:
    json.loads(
        data.decode("ascii"),
        parse_float=reject_noninteger,
        parse_constant=reject_noninteger,
        object_pairs_hook=unique_object,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument(
        "--golden", type=Path,
        default=Path(__file__).resolve().parents[1] / "tests/fixtures/primitive-1000-days.json",
    )
    parser.add_argument("--output", type=Path, help="write the observed normalized JSON for CI evidence")
    args = parser.parse_args()
    try:
        executable = args.executable.resolve(strict=True)
        completed = subprocess.run([str(executable)], capture_output=True, timeout=60, check=False)
        if completed.returncode:
            print(f"Primitive fixture exited {completed.returncode}", file=sys.stderr)
            sys.stderr.write(completed.stderr.decode("utf-8", errors="replace"))
            return 1
        actual = completed.stdout.replace(b"\r\n", b"\n")
        expected = args.golden.read_bytes().replace(b"\r\n", b"\n")
        validate_json(actual)
        validate_json(expected)
        if args.output is not None:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_bytes(actual)
        if actual != expected:
            first_difference = next(
                (index for index, pair in enumerate(zip(actual, expected)) if pair[0] != pair[1]),
                min(len(actual), len(expected)),
            )
            print(
                f"Primitive fixture differs at byte {first_difference}: "
                f"observed {len(actual)} bytes, expected {len(expected)} bytes",
                file=sys.stderr,
            )
            return 1
        print(f"Primitive fixture matches: SHA-256 {hashlib.sha256(actual).hexdigest()}")
        return 0
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"Primitive fixture check failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
