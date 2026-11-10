# Employee portrait art

The current kit uses original frontal vector heads, hair and facial features,
with a restrained illustrated palette shared with the rooms and desktop.
All six heads use the same frontal feature anchors; side-facing source paths
are no longer included. The archived Bitclone CC0 source remains as a reference.
Runtime art is original project work under GPL-3.0-or-later, not AI-generated.

Profiles save 15 part IDs and 12 style IDs. The style array includes body build,
height, clothing cut, pattern, eyes, lips, glasses, pattern color, brows,
facial hair, jacket and tie. Hair, brows and facial hair have independent colors.
Gender only suggests an editable starting look. Appearance never consumes the
simulation RNG or sets aptitude, salary or eligibility.

`python3 tools/prepare_portrait_kit.py --check` verifies the reproducible vector
kit. Rendering uses the actual requested pixel density, with a 2048-pixel cap.
An unspecified image request renders at 480 pixels, rather than a 32px fallback.
Fresh saves retain every part and color; earlier development saves are not a
compatibility target.
