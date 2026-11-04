#!/usr/bin/env python3
"""Offline, standard-library version identity. This tool never publishes or edits versions."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys


DEFAULT_MANIFEST = Path(__file__).resolve().parents[1] / "project-versions.json"
SEMVER = re.compile(
    r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)"
    r"(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?"
    r"(?:\+([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?", re.ASCII)
PRODUCTS = ("game", "editor")
PHASES = ("planning", "development", "release-candidate", "release")
MAX_COMPONENT = 2_147_483_647


class VersionError(ValueError):
    pass


@dataclass(frozen=True)
class Version:
    full: str
    core: tuple[int, int, int]
    prerelease: tuple[str, ...]
    metadata: str

    @property
    def numeric(self) -> str:
        return ".".join(str(value) for value in self.core)


def parse_version(value: object) -> Version:
    if not isinstance(value, str) or len(value) > 256:
        raise VersionError("version must be a SemVer string of at most 256 ASCII characters")
    match = SEMVER.fullmatch(value)
    if not match:
        raise VersionError(f"invalid Semantic Version: {value!r}")
    prerelease = tuple(match[4].split(".")) if match[4] else ()
    if any(part.isdigit() and len(part) > 1 and part[0] == "0" for part in prerelease):
        raise VersionError("numeric prerelease identifiers must not have leading zeros")
    return Version(value, (int(match[1]), int(match[2]), int(match[3])), prerelease, match[5] or "")


def compare_versions(left: str, right: str) -> int:
    """SemVer precedence; build metadata never changes ordering."""
    a, b = parse_version(left), parse_version(right)
    if a.core != b.core:
        return 1 if a.core > b.core else -1
    if not a.prerelease or not b.prerelease:
        return (bool(b.prerelease) > bool(a.prerelease)) - (bool(b.prerelease) < bool(a.prerelease))
    for x, y in zip(a.prerelease, b.prerelease):
        if x == y:
            continue
        if x.isdigit() and y.isdigit():
            return 1 if int(x) > int(y) else -1
        if x.isdigit() != y.isdigit():
            return -1 if x.isdigit() else 1
        return 1 if x > y else -1
    return (len(a.prerelease) > len(b.prerelease)) - (len(a.prerelease) < len(b.prerelease))


def _object(pairs: list[tuple[str, object]]) -> dict:
    result: dict = {}
    for key, value in pairs:
        if key in result:
            raise VersionError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _keys(value: object, expected: set[str], context: str) -> dict:
    if not isinstance(value, dict) or set(value) != expected:
        raise VersionError(f"{context} requires exactly: {', '.join(sorted(expected))}")
    return value


def validate_manifest(data: object) -> dict:
    root = _keys(data, {"manifest_format", "project_id", "phase", "components", "compatibility"}, "manifest")
    if type(root["manifest_format"]) is not int or root["manifest_format"] != 1:
        raise VersionError("unsupported manifest_format; expected integer 1")
    if root["project_id"] != "project-glowing-pancake":
        raise VersionError("unexpected project_id")
    if not isinstance(root["phase"], str) or root["phase"] not in PHASES:
        raise VersionError(f"phase must be one of {', '.join(PHASES)}")
    components = _keys(root["components"], {"game", "editor", "content"}, "components")
    for name, value in components.items():
        parsed = parse_version(value)
        if any(part > MAX_COMPONENT for part in parsed.core):
            raise VersionError(f"{name} numeric version components exceed the supported CMake range")
    compatibility = _keys(root["compatibility"], {"content_schema", "simulation_rules", "save_format"}, "compatibility")
    for name, value in compatibility.items():
        if type(value) is not int or not 1 <= value <= MAX_COMPONENT:
            raise VersionError(f"{name} must be a positive integer at most {MAX_COMPONENT}")
    return root


def load_manifest(path: Path) -> dict:
    raw = path.read_bytes()
    if len(raw) > 65_536:
        raise VersionError("version manifest exceeds 64 KiB")
    try:
        data = json.loads(raw.decode("utf-8"), object_pairs_hook=_object,
                          parse_constant=lambda value: (_ for _ in ()).throw(VersionError(f"invalid JSON constant: {value}")))
    except (UnicodeDecodeError, ValueError, RecursionError) as error:
        raise VersionError(f"invalid UTF-8 JSON manifest: {error}") from error
    return validate_manifest(data)


def product_tag(manifest: dict, product: str) -> str:
    if product not in PRODUCTS:
        raise VersionError("only game/editor have independent product tags")
    return f"{product}-v{manifest['components'][product]}"


def artifact_stem(manifest: dict, product: str, target_os: str, architecture: str) -> str:
    if target_os not in ("macos", "linux", "windows") or architecture not in ("amd64", "arm64"):
        raise VersionError("artifact target must use macos/linux/windows and amd64/arm64")
    product_tag(manifest, product)
    return f"pgp-{product}-{manifest['components'][product]}-{target_os}-{architecture}"


def source_identity(root: Path) -> dict:
    """Configure-time observation only; an exported source tree has unknown Git identity."""
    try:
        def git(*arguments: str) -> str:
            return subprocess.run(["git", "--no-optional-locks", "-C", str(root), *arguments],
                                  check=True, capture_output=True, text=True, timeout=10).stdout.strip()
        if Path(git("rev-parse", "--show-toplevel")).resolve() != root.resolve():
            raise ValueError("not the owning Git root")
        commit = git("rev-parse", "--verify", "HEAD")
        if not re.fullmatch(r"[0-9a-f]{40}|[0-9a-f]{64}", commit):
            raise ValueError("unexpected Git object ID")
        state = "modified" if git("status", "--porcelain", "--untracked-files=normal") else "clean"
        return {"commit": commit, "state": state}
    except (OSError, subprocess.SubprocessError, ValueError):
        return {"commit": "unavailable", "state": "unavailable"}


def identity_errors(manifest: dict, product: str, tag: str, previous: str | None = None) -> list[str]:
    """Only the component identity gate, never package/release qualification."""
    errors = []
    version = parse_version(manifest["components"][product])
    if manifest["phase"] not in ("release-candidate", "release"):
        errors.append("manifest phase is not release-candidate or release")
    if version.core == (0, 0, 0):
        errors.append(f"{product} still has a planning placeholder version")
    if manifest["phase"] == "release" and version.prerelease:
        errors.append("a release-phase product cannot have a prerelease version")
    if tag != product_tag(manifest, product):
        errors.append(f"tag must exactly match {product_tag(manifest, product)}")
    if previous is not None and compare_versions(version.full, previous) <= 0:
        errors.append("version must be newer than this product's previous released version")
    return errors


def cmake_values(manifest: dict, path: Path) -> str:
    values = {"PROJECT_ID": manifest["project_id"], "PHASE": manifest["phase"],
              "MANIFEST_SHA256": hashlib.sha256(path.read_bytes()).hexdigest()}
    for name, value in manifest["components"].items():
        parsed = parse_version(value)
        values[name.upper()] = value
        values[f"{name.upper()}_NUMERIC"] = parsed.numeric
        values[f"{name.upper()}_PRERELEASE"] = ".".join(parsed.prerelease)
    for name, value in manifest["compatibility"].items():
        values[name.upper()] = str(value)
    source = source_identity(path.parent)
    values["SOURCE_COMMIT"] = source["commit"]
    values["SOURCE_STATE"] = source["state"]
    # Every emitted value is an allowlisted ASCII identity, never raw JSON/code.
    if any(not re.fullmatch(r"[A-Za-z0-9_.+-]*", value) for value in values.values()):
        raise VersionError("unsafe CMake identity value")
    return "\n".join(f'set(PGP_VERSION_{key} "{value}")' for key, value in values.items()) + "\n"


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    commands = parser.add_subparsers(dest="command", required=True)
    read = commands.add_parser("read")
    read.add_argument("--format", choices=("json", "cmake"), default="json")
    compare = commands.add_parser("compare")
    compare.add_argument("left")
    compare.add_argument("right")
    for name in ("tag", "artifact", "check-release"):
        command = commands.add_parser(name)
        command.add_argument("--product", choices=PRODUCTS, required=True)
        if name == "artifact":
            command.add_argument("--os", choices=("macos", "linux", "windows"), required=True)
            command.add_argument("--arch", choices=("amd64", "arm64"), required=True)
        elif name == "check-release":
            command.add_argument("--tag", required=True)
            command.add_argument("--previous-version")
    args = parser.parse_args(arguments)
    try:
        if args.command == "compare":
            print(compare_versions(args.left, args.right))
            return 0
        manifest = load_manifest(args.manifest)
        if args.command == "read":
            print(cmake_values(manifest, args.manifest) if args.format == "cmake" else json.dumps(manifest, indent=2), end="\n" if args.format == "json" else "")
        elif args.command == "tag":
            print(product_tag(manifest, args.product))
        elif args.command == "artifact":
            print(artifact_stem(manifest, args.product, args.os, args.arch))
        else:
            errors = identity_errors(manifest, args.product, args.tag, args.previous_version)
            source = source_identity(args.manifest.parent)
            if source["state"] != "clean":
                errors.append("a known clean Git source commit is required")
            remaining = ["validated runtime collection hash and finalized content-to-game version requirements",
                         "save/content migrations and published-version payload-reuse checks",
                         "dependency/source/license manifest and native package/device qualification"]
            print(json.dumps({"product": args.product, "identity_gate_passed": not errors,
                              "identity_errors": errors, "source": source,
                              "release_ready": False, "remaining_gates": remaining}, indent=2))
            # Until later gates have an owning implementation, this command fails closed.
            return 1
        return 0
    except (OSError, VersionError) as error:
        print(f"Version identity error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
