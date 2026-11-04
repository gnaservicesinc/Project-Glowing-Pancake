#!/usr/bin/env python3
"""Regenerate original audio probe fixtures; FFmpeg is an authoring tool only.

The generated 440 Hz test signal is dedicated to CC0-1.0. This script is
GPL-3.0-only under the repository license. It generates no soundtrack.
"""

import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import wave

SAMPLE_RATE = 48_000
FRAMES = 9_600


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ffmpeg", default="ffmpeg", help="installed FFmpeg executable")
    args = parser.parse_args()
    directory = Path(__file__).resolve().parent
    wav_path = directory / "tone.wav"
    ogg_path = directory / "tone.ogg"
    pcm = bytearray()
    for frame in range(FRAMES):
        # A short fade avoids an abrupt start/stop click on the playback probe.
        fade = min(1.0, frame / 480, (FRAMES - 1 - frame) / 480)
        sample = round(4_096 * fade * math.sin(2 * math.pi * 440 * frame / SAMPLE_RATE))
        pcm.extend(struct.pack("<hh", sample, sample))
    with wave.open(str(wav_path), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(SAMPLE_RATE)
        output.writeframes(pcm)
    command = [
        args.ffmpeg, "-nostdin", "-hide_banner", "-loglevel", "error", "-y",
        "-fflags", "+bitexact", "-i", "tone.wav", "-map_metadata", "-1",
        "-fflags", "+bitexact",
        "-c:a", "vorbis", "-strict", "experimental", "-flags:a", "+bitexact",
        "-q:a", "3", "tone.ogg",
    ]
    subprocess.run(command, cwd=directory, check=True)
    version = subprocess.run(
        [args.ffmpeg, "-version"], check=True, capture_output=True, text=True
    ).stdout.splitlines()[0]
    files = {}
    for path in (wav_path, ogg_path):
        data = path.read_bytes()
        files[path.name] = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
    if sum(item["bytes"] for item in files.values()) >= 50_000:
        raise RuntimeError("audio probe fixtures exceeded the 50 KB combined budget")
    provenance = {
        "format": 1,
        "purpose": "M0 local decoding/playback probe; a test tone, not a soundtrack",
        "creator": "Project Glowing Pancake procedural generator, authored with Codex",
        "source": "generate.py; PCM generated from a sine equation, with no sampled source",
        "license": "CC0-1.0",
        "license_url": "https://creativecommons.org/publicdomain/zero/1.0/",
        "generation": {
            "command": "python3 assets/probe/audio/generate.py --ffmpeg /usr/local/bin/ffmpeg",
            "wav": {"frequency_hz": 440, "sample_rate_hz": SAMPLE_RATE, "frames": FRAMES,
                    "channels": 2, "sample_bits": 16, "byte_order": "little-endian", "peak_amplitude": 4096},
            "ogg_encoder_command": ["ffmpeg", *command[1:]],
            "ffmpeg_version": version,
            "note": "FFmpeg is an installed authoring tool; neither its binary nor libraries are game dependencies. Bitexact flags remove volatile encoder/container metadata; different encoder versions may still produce different bytes.",
        },
        "files": files,
    }
    (directory / "provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")
    print(json.dumps(files, indent=2))


if __name__ == "__main__":
    main()
