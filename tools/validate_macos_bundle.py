#!/usr/bin/env python3
"""Validate a local game bundle's structure and actual dynamic-library closure."""
from pathlib import Path
import plistlib
import subprocess
import sys

bundle = Path(sys.argv[1]).resolve()
contents = bundle / "Contents"
with (contents / "Info.plist").open("rb") as file:
    info = plistlib.load(file)
assert info["CFBundlePackageType"] == "APPL"
assert info["CFBundleIdentifier"] == "org.projectglowingpancake.game"
executable = contents / "MacOS" / info["CFBundleExecutable"]
for file in (executable, contents / "Resources/Pancake.icns", contents / "Resources/Menu/horizon.mp4",
             contents / "Resources/qt.conf", contents / "PlugIns/platforms/libqcocoa.dylib"):
    assert file.is_file(), f"Missing bundle resource: {file}"
magic = {b"\xcf\xfa\xed\xfe", b"\xce\xfa\xed\xfe", b"\xca\xfe\xba\xbe", b"\xca\xfe\xba\xbf"}
errors = []
count = 0
for file in contents.rglob("*"):
    if file.is_symlink() or not file.is_file():
        continue
    with file.open("rb") as source:
        if source.read(4) not in magic:
            continue
    count += 1
    install_ids = subprocess.check_output(["/usr/bin/otool", "-D", str(file)], text=True).splitlines()[1:]
    linked = subprocess.check_output(["/usr/bin/otool", "-L", str(file)], text=True)
    for line in linked.splitlines()[1:]:
        if not line.startswith("\t"):
            continue
        dependency = line.strip().split(" (compatibility version", 1)[0]
        if dependency in {value.strip() for value in install_ids}:
            continue
        if dependency.startswith(("/System/Library/", "/usr/lib/")):
            continue
        if dependency.startswith("@rpath/"):
            target = contents / "Frameworks" / dependency.removeprefix("@rpath/")
        elif dependency.startswith("@loader_path/"):
            target = file.parent / dependency.removeprefix("@loader_path/")
        elif dependency.startswith("@executable_path/"):
            target = executable.parent / dependency.removeprefix("@executable_path/")
        else:
            errors.append(f"{file.relative_to(bundle)} requires external library {dependency}")
            continue
        if not target.is_file() or not target.resolve().is_relative_to(contents):
            errors.append(f"{file.relative_to(bundle)} has an unresolved dependency {dependency}")
if errors:
    raise SystemExit("\n".join(errors))
print(f"Validated Mac application resources and {count} Mach-O files; all non-system libraries are bundled.")
