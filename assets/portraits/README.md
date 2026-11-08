# Employee portrait kit v1

Profiles store `portrait: {kit: 1, parts: [...]}`. The fixed 15-byte layer order is
head, hair, nose, ears, mouth, chin, neck, eyes, eyebrows, facial hair, glasses,
skin color, hair color, clothing color, background. JSON integer encoding is
slightly larger; no pixel images, SVG documents, paths or URLs enter saves.
Existing IDs retain their meanings. A new incompatible style needs a new kit
version and a retained renderer for earlier saves.

Appearance is generated once on application/identity creation through a private
visual stream. It does not consume simulation RNG or affect skills, pay, gender,
pronouns or eligibility. Hires copy the applicant's exact profile. Older development
profiles without a recipe receive a repeatable fallback from their existing ID
and name; importing preserves the source save.

The game composes trusted bundled vector fragments and renders at each displayed
portrait's device-pixel size. Only visible portrait size drives the texture size,
with a 2048-pixel safety cap. Card portraits, founder HUD and enlarged profile use
the same recipe. Keyboard activation and pointer selection open the profile;
hover/focus styling respects reduced motion. Dense and narrow windows retain a
scrollable layout. No network retrieval is performed in the game.

## Sources and preparation

`kit-v1.json` uses selected normalized head and rear-hair paths from **Bitclone
Basic Face Kit 2**, VexStrips (with source remix credit to future), uploaded
2018-05-19: https://openclipart.org/detail/302186/bitclone-basic-face-kit-2.
The user supplied the source SVG, retained byte-for-byte in `sources/`.
The source's publication page and Openclipart's CC0 policy were checked 2026-11-07:
https://openclipart.org/share and https://creativecommons.org/publicdomain/zero/1.0/.
Those selected paths retain their CC0 status. Complementary front hair, facial
features, necks, clothing and palette are original code-authored project art under
the repository license. This is procedural vector composition, not image-model
output. Other files from the supplied SVG folder are references and are not bundled.

`python3 tools/prepare_portrait_kit.py --check` reproduces the reviewed kit from
the retained source and fails if its source bytes change. The script records the
source SHA-256 and reviewed geometry bounds. SVG editor metadata, executable
content, filters, external resources and statistical/source examples are excluded
from the bundled kit. `assets/manifest.json` records provenance and modifications.
