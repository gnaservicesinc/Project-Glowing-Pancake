# Pinned optional audio sources

These are original upstream bytes. The project selects MIT-0 for miniaudio
and MIT for stb_vorbis; upstream offers additional choices. Preserve the
complete license files and source license notices in corresponding-source
and license bundles. The root GPLv3 code license does not replace them.

`sources.json` records immutable source URLs, commits, sizes and SHA-256
fingerprints. No downloading occurs during a game build or at runtime.

miniaudio is header-only when `MINIAUDIO_IMPLEMENTATION` is defined in one
translation unit. Its implementation is already inside `miniaudio.h`; no
separate `miniaudio.c` is required. Include stb_vorbis declarations before
the miniaudio implementation to enable Vorbis, following upstream's header
instructions. Restrict enabled decoders and external inputs in the adapter;
vendoring does not itself establish a playback-only configuration.

Original short decoding/playback fixtures and their generation provenance
live under `assets/probe/audio/`. Installed FFmpeg creates the Ogg fixture
only as a development authoring tool; no FFmpeg executable or library is
vendored here or required by the game.
