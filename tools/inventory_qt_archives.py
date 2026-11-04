#!/usr/bin/env python3
"""Inspect pinned Qt installer metadata/sidecars without downloading SDK archives.

This development-only tool uses the network. Nothing imports it into the game.
Output is an observation, not proof of a successful install or native package.
"""

import concurrent.futures
import hashlib
import json
import re
import sys
import urllib.request
import xml.etree.ElementTree as ET

BASE = "https://download.qt.io/online/qtsdkrepository"
VERSION = "6.12.0"
TARGETS = (
    ("macos-arm64", "mac_x64", "qt6_6120", "clang_64"),
    ("linux-amd64", "linux_x64", "qt6_6120", "linux_gcc_64"),
    ("linux-arm64", "linux_arm64", "qt6_6120", "linux_gcc_arm64"),
    ("windows-amd64", "windows_x86", "qt6_6120_msvc2022_64", "win64_msvc2022_64"),
    ("windows-arm64", "windows_arm64", "qt6_6120", "win64_msvc2022_arm64"),
)
SELECTED_ARCHIVES = {"qtbase", "qtdeclarative", "qtshadertools"}


def read_small(url, limit=2_000_000):
    with urllib.request.urlopen(url, timeout=30) as response:
        data = response.read(limit + 1)
    if len(data) > limit:
        raise ValueError(f"metadata exceeded {limit} bytes: {url}")
    return data


def inspect_target(target):
    name, host, repository, architecture = target
    root_url = f"{BASE}/{host}/desktop/qt6_6120/{repository}/"
    xml_url = root_url + "Updates.xml"
    data = read_small(xml_url)
    root = ET.fromstring(data)
    packages = []
    for node in root.findall("PackageUpdate"):
        package = node.findtext("Name", "")
        if package not in (
            f"qt.qt6.6120.{architecture}",
            f"qt.qt6.6120.addons.qtshadertools.{architecture}",
        ):
            continue
        version = node.findtext("Version", "")
        archives = []
        for archive in node.findtext("DownloadableArchives", "").split(","):
            archive = archive.strip()
            if not archive:
                continue
            entry = {"name": archive}
            if archive.split("-", 1)[0] in SELECTED_ARCHIVES:
                archive_url = f"{root_url}{package}/{version}{archive}"
                checksum = read_small(archive_url + ".sha256", 512).decode().split()[0]
                if not re.fullmatch(r"[0-9a-fA-F]{64}", checksum):
                    raise ValueError(f"invalid SHA256 sidecar: {archive_url}")
                entry.update(url=archive_url, published_sha256=checksum.lower())
            archives.append(entry)
        packages.append({
            "package": package,
            "version": version,
            "display_name": node.findtext("DisplayName"),
            "release_date": node.findtext("ReleaseDate"),
            "archives": archives,
        })
    if len(packages) != 2:
        raise ValueError(f"missing base/shader-tools package for {name}")
    return {
        "target": name,
        "host_directory": host,
        "architecture": architecture,
        "metadata_url": xml_url,
        "metadata_sha256": hashlib.sha256(data).hexdigest(),
        "metadata_bytes": len(data),
        "packages": packages,
    }


def main():
    results = []
    failed = False
    with concurrent.futures.ThreadPoolExecutor(max_workers=5) as executor:
        futures = [(target, executor.submit(inspect_target, target)) for target in TARGETS]
        for target, future in futures:
            try:
                results.append(future.result())
            except Exception as error:
                failed = True
                results.append({"target": target[0], "error": str(error)})
    print(json.dumps({
        "qt_version": VERSION,
        "note": "Metadata and published SHA256 observations only; SDK archives were not fetched or verified against these checksums.",
        "targets": results,
    }, indent=2))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
