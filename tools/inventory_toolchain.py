#!/usr/bin/env python3
"""Read installed macOS toolchain/Qt metadata and run disposable CLI probes.

No SDK installation, global configuration, GUI launch or network access occurs.
Scratch executables are removed before this process exits.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile


def run(args: list[str], *, env: dict[str, str] | None = None) -> str:
    result = subprocess.run(args, env=env, check=False, text=True, timeout=60,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        raise RuntimeError(f"{args[0]} exited {result.returncode}:\n{result.stdout}{result.stderr}")
    return (result.stdout + result.stderr).strip()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qt-prefix", type=Path,
                        default=Path("/opt/Qt/6.12.0/macos"))
    args = parser.parse_args()
    if platform.system() != "Darwin":
        parser.error("This inventory probes the existing macOS SDK; other targets use native CI.")
    root = Path(__file__).resolve().parents[1]
    developer_dir = Path(os.environ.get("DEVELOPER_DIR") or
                         run(["/usr/bin/xcode-select", "-p"])).resolve(strict=True)
    env = dict(os.environ, DEVELOPER_DIR=str(developer_dir))
    compiler = Path(run(["/usr/bin/xcrun", "--find", "clang++"], env=env))
    sdk = Path(run(["/usr/bin/xcrun", "--show-sdk-path"], env=env))
    for path in (compiler.resolve(strict=True), sdk.resolve(strict=True)):
        if not path.is_relative_to(developer_dir):
            raise RuntimeError(f"Selected compiler/SDK escapes selected Xcode: {path}")
    qt = args.qt_prefix.resolve(strict=True)
    required_modules = ["Core", "Gui", "Qml", "Quick", "QuickControls2", "Network"]
    report = {
        "os": run(["/usr/bin/sw_vers", "-productVersion"]),
        "architecture": platform.machine(),
        "free_gib": round(shutil.disk_usage(root).free / 1024**3, 2),
        "developer_dir": str(developer_dir),
        "compiler": str(compiler),
        "compiler_version": run([str(compiler), "--version"], env=env),
        "sdk": str(sdk.resolve()),
        "sdk_version": run(["/usr/bin/xcrun", "--show-sdk-version"], env=env),
        "cmake": run(["cmake", "--version"]).splitlines()[0],
        "ninja": run(["ninja", "--version"]),
        "qt_prefix": str(qt),
        "qt_version": run([str(qt / "bin/qmake"), "-query", "QT_VERSION"]),
        "qt_modules": {name: (qt / f"lib/cmake/Qt6{name}/Qt6{name}Config.cmake").is_file()
                       for name in required_modules + ["Multimedia"]},
        "sboms": [],
    }
    for module in ("qtbase", "qtdeclarative", "qtimageformats"):
        path = qt / f"sbom/{module}-{report['qt_version']}.spdx.json"
        data = path.read_bytes()
        document = json.loads(data)
        report["sboms"].append({
            "path": str(path), "sha256": hashlib.sha256(data).hexdigest(),
            "package_count": len(document["packages"]),
            "packages": [{"name": p["name"], "version": p.get("versionInfo"),
                          "license": p.get("licenseConcluded")}
                         for p in document["packages"]],
        })
    scratch = root / ".work"
    scratch.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="m0-toolchain-", dir=scratch) as directory:
        work = Path(directory)
        core = work / "core.cpp"
        core.write_text("#include <array>\n#include <span>\n#include <ranges>\n"
                        "#include <numeric>\n#include <iostream>\n"
                        "int main(){ std::array a{10,14,18}; std::span s{a}; "
                        "auto v=s | std::views::reverse; "
                        "auto n=std::accumulate(v.begin(),v.end(),0); "
                        "std::cout << n << '\\n'; return n==42 ? 0 : 1; }\n")
        flags = ["-std=c++20", "-O2", "-isysroot", str(sdk),
                 "-mmacosx-version-min=14.4", "-arch", "arm64"]
        core_command = [str(compiler), *flags, str(core), "-o", str(work / "core")]
        run(core_command, env=env)
        report["core_probe"] = {"command": core_command,
                                "output": run([str(work / "core")], env=env),
                                "architecture": run(["/usr/bin/file", str(work / "core")])}
        qt_source = work / "qt_core.cpp"
        qt_source.write_text("#include <QtCore/QString>\n#include <iostream>\n"
                             "int main(){ std::cout << QStringLiteral(\"Qt Core CLI probe\")"
                             ".toStdString() << '\\n'; }\n")
        qt_command = [str(compiler), *flags, str(qt_source), "-o", str(work / "qt-core"),
                      "-F", str(qt / "lib"), "-I", str(qt / "lib/QtCore.framework/Headers"),
                      "-I", str(qt / "lib/QtCore.framework"), "-framework", "QtCore",
                      f"-Wl,-rpath,{qt / 'lib'}"]
        run(qt_command, env=env)
        report["qt_core_probe"] = {"command": qt_command,
                                   "output": run([str(work / "qt-core")], env=env),
                                   "linkage": run(["/usr/bin/otool", "-L", str(work / "qt-core")])}
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
