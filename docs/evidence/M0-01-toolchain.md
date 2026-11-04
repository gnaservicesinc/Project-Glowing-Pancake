# M0-01 — Toolchain and platform inventory

Observed 2026-11-03. This closes the inventory task, not the graphics/audio,
five-runner packaging or clean-device qualification gates. No SDK was installed,
global tool setting changed, GUI launched or game package published.

## Local compiler and linkage

Reproduce from the repository with `python3 tools/inventory_toolchain.py`.
The tool emits JSON and removes its disposable CLI sources/executables under
`.work/`; it performs no network access. It needs the existing Qt installation.

| Item | Observed result |
| --- | --- |
| Native host | macOS 27.3, arm64; Mac14,13, 64 GiB memory |
| Free storage | 23.35 GiB during this inventory; transient, not a reservation |
| CMake / Ninja | 4.4.4 / 1.13.2.git.kitware.jobserver-pipe-1 |
| PATH C++ compiler | `/usr/local/bin/clang++`, LLVM 23.1.3; not selected for Qt |
| Selected Xcode | `/Applications/Xcode-beta.app/Contents/Developer` |
| Apple compiler | Selected Xcode's `Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++`; Apple Clang 21.0.0 (clang-2100.4.1.1) |
| SDK | Selected Xcode's `Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk`, version 27.3 |
| C++20 test | `std::span`, ranges and integer accumulation compiled/linked with `-std=c++20 -O2 -arch arm64 -mmacosx-version-min=14.4`; ran and returned `42` / exit 0 |
| Qt linkage test | CLI using `QString` compiled/linked and ran with Qt Core 6.12.0; `otool -L` showed `@rpath/QtCore.framework/Versions/A/QtCore`, system libc++ and libSystem |

`/Applications/Xcode.app` is a symlink to `Xcode-beta.app`. The earlier apparent
compiler/SDK root mismatch is lexical: canonical paths share one Xcode root.
The wrapper must canonicalize the selected developer directory, resolve both
compiler and SDK through it, reject an actual escape/mismatched cache, and avoid
changing `xcode-select`. This remains a development beta SDK; minimum-OS runtime
acceptance and release-toolchain selection still require CI/device evidence.

## Qt modules and preliminary dependency register

`/opt/Qt/6.12.0/macos/bin/qmake -query QT_VERSION` reports `6.12.0`.
Core, Gui, Network, Qml, Quick and QuickControls2 CMake configs are present;
Multimedia is absent. `lipo -archs` reports `x86_64 arm64` for Qt Core.
`mkspecs/qconfig.pri` specifies universal architectures, macOS deployment 14.4,
Apple Clang 17 build metadata, SDK 26.0, and accepted SDK range 15–27.

The register below is a **deployment candidate**, seeded from installed SPDX
JSON package records and observed framework linkage. No game payload exists to
reconcile yet. M0-03/04 must match the actually copied frameworks/plugins/QML,
including implicit imports, to their SBOM records and notices. Do not copy the
entire SDK or infer that every installed module ships.

| Candidate | Installed evidence / license path | Deployment intent |
| --- | --- | --- |
| Qt Core, Gui, Network, OpenGL, DBus | `qtbase-6.12.0.spdx.json`; module records list commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only | Dynamic framework closure; Gui links DBus; Quick links OpenGL even with Metal selected |
| Qml, QmlMeta, QmlModels, QmlWorkerScript, Quick, QuickTemplates2, QuickControls2, Basic style/implementation/plugins | `qtdeclarative-6.12.0.spdx.json`; same module alternatives | Start with one Basic controls style; import scanner/deployment output decides exact closure |
| Cocoa platform / JPEG plugin | Qt base SPDX records, same alternatives; Cocoa attribution BSD-3-Clause | Cocoa platform needed on Mac; JPEG plugin and builtin PNG for local fixtures |
| PCRE2 10.48 / SLJIT | Qt base: PCRE2-specific BSD license reference / BSD-2-Clause | Include exact bundled notices when Core closure ships |
| libpng 1.6.58 / libjpeg-turbo 3.2.0 | Qt base: `Libpng AND libpng-2.0` / `IJG AND BSD-3-Clause` | Needed image decoder attribution, not a claim all 12/16-bit variants ship |
| FreeType 2.14.3 / HarfBuzz 14.5.0 | Qt base: `FTL OR GPL-2.0-only` / MIT | Retain actual font-rendering notices when linked/bundled |
| Core/QML embedded third-party code | SPDX attribution entries include Unicode-3.0, CC0, MIT, Apache-2.0, BSD and other license references | Preserve complete selected-module notices; this table is not a replacement |
| System frameworks, libc++, libSystem, system zlib | `otool -L` of Qt Gui/Quick/Core | Host dependencies; no copying of Apple system frameworks |
| Qt tools (`moc`, `rcc`, deployment/import tools) | Qt base/declarative SPDX: GPL-3.0-only WITH Qt-GPL-exception-1.0 alternative | Build tools, excluded from game; preserve source/build notices separately |
| Qt Multimedia / FFmpeg | Multimedia unavailable; no dependency adopted | Silent build works without either |
| QtImageFormats extras, SVG, WebEngine, Graphs, Location | Extra imageformats SBOM inspected; unrelated modules not selected | Exclude unless a future reviewed feature justifies them |

For the project's GPLv3 distribution, use the offered GPLv3 route for selected Qt
modules and preserve their distinct terms and matching source/notices; do not
relabel Qt or third-party code as project-owned. Actual installed SPDX records
are the detailed evidence, including `NOASSERTION` and license references that
need resolution before shipping. [Qt licensing](https://doc.qt.io/qt-6/licensing.html)
describes module alternatives and SBOM availability.

Parsed installed source fingerprints:

| SPDX JSON under `/opt/Qt/6.12.0/macos/sbom/` | Packages | SHA-256 |
| --- | --- | --- |
| `qtbase-6.12.0.spdx.json` | 145 | `fe5036f8aad3b9d3cb848a7f34731260a95b271dba4f1b59a6bf5882e6ba8db5` |
| `qtdeclarative-6.12.0.spdx.json` | 165 | `48c22e93885b5280972135307da4ec9c9920c5b0007e109d8829c1cd0aee02f3` |
| `qtimageformats-6.12.0.spdx.json` | 15 | `dd74996a27f0ce57ae5a0f2f69a76aa07ba5b10d529e1378acf7b9fef484e062` |

## Exact five-target acquisition

All five Qt 6.12.0 base packages and Shader Tools addon were found in official
installer metadata. Package version is `6.12.0-0-202609280346`, release date
2026-09-28. Each base package explicitly contains `qtbase` and `qtdeclarative`.
The [checked observation](M0-01-qt-archives.json) records full metadata URLs,
metadata SHA-256, exact archive filenames/URLs and 15 published archive SHA-256
sidecars. Reproduce with `python3 tools/inventory_qt_archives.py`. Only metadata
and sidecars were fetched; archive bodies were neither downloaded nor verified.

| Target | Installer host / architecture key | Qt binary build ABI | Proposed native runner |
| --- | --- | --- | --- |
| macOS arm64 | `mac_x64` / `clang_64` | macOS 26, Clang, universal x86_64+arm64 | `macos-15` (arm64) |
| Linux amd64 | `linux_x64` / `linux_gcc_64` | RHEL 9.6 GCC x86_64 | `ubuntu-24.04` |
| Linux arm64 | `linux_arm64` / `linux_gcc_arm64` | Ubuntu 24.04 GCC AARCH64 | `ubuntu-24.04-arm` |
| Windows amd64 | `windows_x86` / `win64_msvc2022_64` | Windows 11 24H2 MSVC2022 x86_64 | `windows-2025` |
| Windows arm64 | `windows_arm64` / `win64_msvc2022_arm64` | Windows 11 23H2 MSVC2022 AARCH64 | `windows-11-arm` |

Installer repository roots have an additional directory below `qt6_6120`; the
Windows amd64 leaf is `qt6_6120_msvc2022_64`. Querying only the parent
`Updates.xml` gives 404 and must not be interpreted as an unavailable Qt release.
The JSON contains the exact working paths.

CI can acquire the pinned archive set from those URLs, check the recorded
upstream SHA-256 after download, then perform the
required Qt relocation/configuration and dependency checks. An aqt-based route
must first verify its current native host/architecture mapping; it has not been
installed or tested here. Source-build fallback belongs on native CI, using a
pinned Qt 6.12 source revision/archive, exact modules and fresh SBOMs; no fallback
or downgrade is currently needed on archive-availability grounds. The official
[source archive](https://download.qt.io/archive/qt/6.12/6.12.0/single/qt-everywhere-src-6.12.0.tar.xz)
is listed with published SHA-256
`98ff4f44bac6ec3e1e62ee2a4316ae0e3d15badb015d753cc0268a20db52f165`;
its body was not downloaded.

Archive labels establish intended ABI, not exact deployed compiler/CRT/glibc
requirements. M0-04 must inspect actual libraries and test those requirements on
Ubuntu 24.04 and the selected MSVC native environments. Metadata also lists ICU,
Wayland and Windows compiler/OpenGL helper archives; payload dependency scanning
must decide which are needed and add their licenses/checksums. Do not assume the
15 initial archives form the complete redistributed dependency closure.

Runner labels are listed in the [GitHub hosted-runner reference](https://docs.github.com/en/actions/reference/runners/github-hosted-runners).
These standard labels document 14 GB runner SSD storage; scope installs/caches
and artifacts accordingly. Availability/documentation does not prove the account can schedule a job or that
headless runners have usable physical GPU/audio access. No workflow ran here.

## Optional audio candidate and remaining qualification

M0-03's candidate is [miniaudio 0.11.25](https://github.com/mackron/miniaudio/tree/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d)
at commit `9634bedb5b5a2ca38c1ee7108a9358a4e233f14d`, with the offered MIT-0
license, plus [stb_vorbis 1.22](https://github.com/nothings/stb/blob/d14d46f6b44456eb88beac58cba833a4135d4c89/stb_vorbis.c)
at `d14d46f6b44456eb88beac58cba833a4135d4c89`, with its MIT option. These are
pins for evaluation, not adopted runtime dependencies. No source is vendored.
Probe explicit playback-only initialization, disabled unwanted decoders/encoding,
bounded PCM WAV/Ogg local inputs and native backends (CoreAudio, WASAPI,
ALSA/PulseAudio). Confirm exact flags, Ogg integration, mute/device loss, absence
of capture permission requests and unavailable-device silent behavior at M0-03.
The build must remain silent without these libraries or Qt Multimedia.

Only this native Mac is evidenced as available. Linux/Windows physical devices,
especially ARM64, remain unconfirmed; no contact or procurement occurred. M0-03
still owns rendered Metal/UI/resource-guard/audio evidence; M0-04 owns five native
CI package runs, downloaded-archive verification and deployed closure/source/license evidence;
M9 owns clean physical-device support qualification. Large SDK/native package
staging stays on Actions. The coherent toolchain and actual small CLI linkage
allow M0-02 to proceed without waiting for those later gates.
