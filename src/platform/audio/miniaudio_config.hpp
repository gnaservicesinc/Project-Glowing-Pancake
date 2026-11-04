#pragma once

// Playback of our already-decoded float PCM only. No file/codec/engine paths.
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_WAV
#define MA_NO_MP3
#define MA_NO_FLAC
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MA_NO_GENERATION
#define MA_NO_STDIO
#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#if defined(__APPLE__)
#define MA_ENABLE_COREAUDIO
#elif defined(_WIN32)
#define MA_ENABLE_WASAPI
#elif defined(__linux__)
#define MA_ENABLE_ALSA
#define MA_ENABLE_PULSEAUDIO
#endif
