# Original menu assets

`horizon.jpg`, `horizon.mp4`, `icon.png`, `Pancake.icns` and `select.wav`
are original procedural work created with Codex assistance for Project Glowing
Pancake, 2026-11-07, under the repository's GPL-3.0 license. The checked-in code
draws the artwork and synthesizes the effect. No third-party source art, fonts,
recordings, image-generation model or downloaded media was used.

Reproduce with `python3 tools/create_menu_assets.py` on macOS (development-only
Pillow and the macOS SDK/iconutil). The MP4 is a silent ten-second H.264 motion loop
derived from the still image. The WAV is a short synthesized selection effect,
not soundtrack music. Existing licensed soundtrack excerpts retain their
individual credits in `assets/audio/CREDITS.json`.
