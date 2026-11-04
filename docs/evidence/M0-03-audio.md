# M0-03 — Local audio adapter evidence

Observed 2026-11-03 on the native arm64 Mac/compiler/SDK recorded in
[M0-01](M0-01-toolchain.md). This is an audio subtask report; it does not close
the entire graphics/resource/deployment milestone or qualify other platforms.

## Implemented behavior

`pgp::audio::Player` is a Qt-free presentation adapter, outside the simulation
core. `pgp_audio`/`pgp::audio` is added only by the desktop build. Its public
header separates local loading, explicit playback, stop, volume/mute, status and
device notifications. Adding future presentation features does not change the
simulation or require embedding audio code in QML. All controls belong to the
owning/UI thread; device-loss notification changes atomic state and is safe from
a platform callback.

The default `PGP_ENABLE_AUDIO=OFF` build decodes and validates local fixtures but
never compiles/opens a native audio device. `PGP_ENABLE_AUDIO=ON` additionally
compiles miniaudio and enables explicit playback. Construction and `load()` never
open a device in either configuration. Only `play()` may open one; failed native
initialization/start and later device loss preserve a successful silent fallback.
Build and installation do not play audio.

Input files must resolve canonically beneath the selected asset directory;
parent traversal, outside absolute paths, symlink escapes, remote URL/UNC forms
and non-regular files are refused. Only `.wav` (RIFF PCM16, mono/stereo) and
`.ogg` (Vorbis mono/stereo) are accepted, at 8–96 kHz. Encoded input is capped at
4 MiB, decoded duration at 30 seconds, Vorbis setup allocation at 2 MiB and
decoded float PCM at 5,760,000 samples maximum. RIFF lengths/alignment, duplicate
format/data chunks, truncated input and inconsistent Vorbis duration are checked.
Failed loads preserve the previously validated clip.

Volume is bounded to 0–1; non-finite values become zero. Mute preserves the
remembered volume while silencing and advancing PCM. Native callbacks only mix
already-decoded PCM and update atomic state; they never initialize, stop or
uninitialize a device. Notification/explicit loss selects silence; teardown
happens on the owning thread. Automatic device reacquisition is deferred.

## Dependencies, provenance and feature restrictions

[The dependency manifest](../../third_party/audio/sources.json) records immutable
commits, original URLs, exact bytes and SHA-256 for unmodified miniaudio 0.11.25,
stb_vorbis 1.22 and their full licenses. The selected alternatives are MIT-0 and
MIT respectively; these retain their own identities alongside project GPLv3.
No package manager or network fetch runs during the build.

`miniaudio_config.hpp` disables its decoding, encoding, WAV/MP3/FLAC, stdio,
engine, node graph, resource manager and signal-generation APIs. Its native
backend allowlist is CoreAudio on Mac, WASAPI on Windows and ALSA/PulseAudio on
Linux. Vorbis alone is decoded from bounded memory through stb; its stdio and
push-data APIs are disabled. The WAV reader is project-owned and accepts PCM16
only. The native archive's exported symbol inspection found no `ma_decoder_*`,
`ma_encoder_*` or dr_mp3/dr_flac/dr_wav APIs.

Every adapter initialization explicitly requests `ma_device_type_playback`; the
adapter has no recording, loopback or capture-enumeration interface. Upstream
miniaudio combines capture/playback device internals and offers no independent
compile-time capture-removal switch; those upstream facilities remain in the
native library, but the application does not request them. No microphone
entitlement/permission request is added. This does not claim a process-wide
sandbox against future native code.

[Fixture provenance](../../assets/probe/audio/provenance.json) and the retained
generator identify the original 440 Hz faded stereo tone, its WAV and its genuine
Vorbis encoding, with CC0 provenance and reproducible hashes. It is a test tone,
not a generated soundtrack. Total fixture size is 42,698 bytes. Installed FFmpeg
was used only as an authoring tool; no FFmpeg binary/library or Qt Multimedia
dependency enters the adapter. The encoder adds Ogg padding, so WAV/Ogg frame
counts are deliberately not asserted equal.

The upstream [miniaudio API/source](https://github.com/mackron/miniaudio/blob/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d/miniaudio.h)
was checked for playback initialization, backend/feature switches and the
callback prohibition on device lifecycle calls. Its MIT-0 license and
[stb license](https://github.com/nothings/stb/blob/d14d46f6b44456eb88beac58cba833a4135d4c89/LICENSE)
were read before adoption.

## Verification and limits

Both Release configurations compiled successfully with the selected Apple Clang
21 toolchain. Project sources use the repository's warnings-as-errors policy;
third-party implementation translation units are isolated from that policy.
An isolated ignored CMake harness included the same `src/platform/audio` target
while the parent desktop probe was being integrated.

| Check | Actual result |
| --- | --- |
| `audio.local-playback-contract`, native option OFF | Passed |
| Same contract, native option ON | Passed |
| WAV/Ogg decode and metadata, unsupported/mislabelled codecs, truncated envelopes, PCM24/float rejection, 4 MiB and 30-second limits | Passed |
| Canonical parent/absolute/symlink escapes and remote URL/file hosts | Passed on Mac; test reports a skip on hosts lacking symlink privileges |
| Failed-load preservation; remembered mute/volume, non-finite/bounded gain, sample mixing/end-of-buffer silence | Passed |
| Deterministic unavailable/lost-device fallback | Passed without hardware access in ordinary CTest |
| Explicit `pgp-audio-test --native-play`, native option ON | Opened `Core Audio`; normal clip completed after 300 ms with `playing=0`, `device_lost=0`; muted replay and synthetic loss fallback passed |
| Same explicit command, option OFF | Reported `silent (native audio not compiled)` and stayed non-playing |
| Native executable linkage | Only CoreFoundation, CoreAudio, AudioToolbox, libc++ and libSystem; no Qt Multimedia/FFmpeg |
| Source/license/fixture hash verification | Every manifest entry matched local bytes |

The manual native invocation is a machine-observed initialization/callback test,
not human confirmation of audible quality. Real cable/device removal and OS
permission-dialog observation were not performed; device loss above uses the
same deterministic notification seam. Windows/Linux native backends, clean
package relocation, the base game's dependency/license closure and physical
device acceptance remain M0-04/M9 gates. Parent app evidence separately covers
the explicit UI button that extracts the bundled fixture and invokes playback.

After configuring a desktop build, reproduce ordinary checks with
`cmake --build <build> --target pgp-audio-test` and
`ctest --test-dir <build> -R audio.local-playback-contract --output-on-failure`.
Configure a separate out-of-source build with `-DPGP_ENABLE_AUDIO=ON` for the
optional native path. Hardware is opened only by manually invoking
`<build>/bin/pgp-audio-test --native-play`, never by CTest.
