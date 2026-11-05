# Content, art, audio, and provenance

Baseline 1.2 · No production assets have been obtained yet

## Writing direction

Write a management game with human stakes and occasional dry humor. Executives have specific goals and incomplete information; neither safety-focused staff nor ambitious researchers are caricatures. Consequences follow actions and context. Avoid a narrator announcing the correct moral answer or turning every choice into profit versus goodness.

Use fictional companies, executives, publications, political parties, and advocacy groups. Real technical concepts and geography may appear where useful. Do not copy Plutocracy's art, UI composition, text, logos, or mechanics. Corporate policies can refer abstractly to lawful verified-adult use, but no sexualized material, suggestive imagery, or explicit content appears. Keep violence, catastrophic events, and substance references brief and non-descriptive; no gore, obscenity, hate speech, romanticized violence, or practical wrongdoing instructions.

A severe event should fit a restrained bulletin: a service has been suspended, institutions have failed, or a campaign has ended. Show operational/social consequences and the causal review without graphic scenes or trauma-focused prose. Staff are people, not expendable numerical casualties. Fictional unethical choices can have consequences without depicting prohibited content.

All platform/store content rules must be rechecked near publication. The project's own stricter tone applies even when a platform would permit more. Store metadata must reflect the actual build and supported platforms. [Itch quality guidelines](https://itch.io/docs/creators/quality-guidelines).

## Content coverage

| Content | First playable M3 | Initial full 1.0 budget |
| --- | --- | --- |
| Main room sets | Office, Lab; other work in panels | Seven main sets + four contextual sets |
| Research | Six meaningful nodes | 60 nodes across seven tracks |
| Dataset offers | Four with distinct tradeoffs | 24 offer families with renewable/scaled variants |
| Hardware | Small leased pool; one replacement/upgrade choice | 18 families across reference and speculative eras |
| Products | One enterprise contract channel | 12 templates across six model channels; wholesale compute contracts alongside them |
| Rivals | Two startups + bounded heavyweight warning | Six domestic, six international, three heavyweights scheduled |
| Executive templates | Three key relationships | 24 templates, combined with local fictional name/trait generation |
| Events | Eight baseline definitions with selected story enrichment | 120 definitions across systemic rules and richer stories, including recovery and opportunity events |
| Visual symbols | About 25 original/simple icons | About 60 reusable icons |
| Music | Silent or one cleared test track | Five ambient, two tension/event tracks; short cues as needed |

The 120-event budget allocates roughly 20 research/data, 20 product/customer, 20 people/board, 20 finance/competition, 20 infrastructure/community, and 20 policy/world definitions. At least one-third should offer opportunities or recovery rather than new punishment. These are coverage budgets for built-in rules and richer stories together, not a requirement to manually write 120 elaborate branching stories. Committed systemic facts generate ordinary news/status messages through localized templates. Coding agents create the base rules, seeded/scheduled world incidents and initial content during repository development; human stories augment them. The [event-system coverage matrix and AI assignments](EVENT_SYSTEM.md) define the eight-flow playable baseline and modular extension checks. No runtime model or live news feed is required.

Every strategic path needs content: profitable specialist, open ecosystem, frontier lab, public-service partnership, infrastructure provider, and authority-seeking institution. Each has an early opportunity, an operational problem, a late-game scaling issue, and consequences that connect to at least two other systems.

## Graphical content production

Human authors use the separately downloaded [Content Studio](CONTENT_EDITOR.md) to browse and maintain the repository's current content. A categorized library links events/flows, scenarios, research, hardware, people, policies, localization and asset records. Trigger boxes connect to ordered effects and events; node graph, forms and outline expose one definition with searchable references and useful scope/unit/tooltips. Ordinary content editing does not require scripts or raw JSON. The base game contains only reviewed runtime data. E1a first supplies complete forms/outline and inspection for slice events; E1b completes the linked-box canvas and graphical finalizer before M4.

Built-in and AI-assisted definitions are first-class entries in the same library, with source/authoring provenance where relevant. Engineering fixtures may be created before Studio exists; reviewed slice content is inspected, managed and finalized through the E1a pipeline. Human creation of every event is not a prerequisite. Human review remains responsible for meaning, clarity and balance, and selected stories can attach to a baseline occurrence without duplicating its mechanics.

Trigger truth/path/scope inspection and before/after effect previews are mandatory. Authors can test threshold boundaries, inspect affected targets and timed behavior, and catch inverted signs or wrong references. Human writing/review remains responsible for story meaning and balance; a valid graph is not an automatically good event. Finalized batches update versions through [the shared versioning workflow](VERSIONING.md), without publishing a game.

## Event and flow authoring contract

Each event needs:

1. A stable ID and definition revision within a versioned content collection, localization keys, permitted era/scenario and content tags.
2. Trigger facts based on actual simulation state, with prerequisites and an explanatory cause visible to the player.
3. A concrete situation or truthful news template; an actionable event has a small set of distinct choices, including a reasonable defer/decline path where applicable. Informational bulletins need no artificial choice.
4. Explicit lifecycle (once, recurring, interval, scheduled or continuous), relative/fixed anchor, occurrence scope, overlap/cooldown/expiry settings, per-effect typed targets, ordered links and due dates; no raw arbitrary code. Clearly identify terms still being negotiated.
5. Conditional delayed consequences tied to the actual decision and later conditions, not an invisible moral score.
6. A test fixture for eligibility/path/targets, threshold and effect direction, each choice/result, recurrence/continuous entry and exit, save/reload while pending, and the default at a disclosed deadline. Each state-changing segment produces an actual before/after trace.

The [supplier example](examples/supplier-event.example.json) demonstrates a trigger and branching negotiation opportunity with actual-result nodes. The [lifecycle gallery](examples/flow-lifecycles.example.json) and [impact fixture](examples/trigger-impact.example.json) illustrate timing and direction checks. These are not complete production schemas. Do not mechanically grant the contract just because the event opened a negotiation. Decision text and mechanical effects must agree; a choice labeled no commitment must not spend money. All authored money uses constant-2014 cents. Presentation urgency (required decision, actionable inbox, informational digest) is explicit and uses actual simulated deadlines. News/dialogue/hidden presentation is independent of lifecycle, so a recurring or continuous rule does not need a special unrelated event language.

Baseline detectors and story layers share a source occurrence/receipt. Declare whether a story replaces its presentation or follows it; neither path may reapply the system result. News may report only known committed results, with uncertainty disclosed. Seeded world incidents need explicit eligibility, chance, bounds, cooldown and expiry; they do not invent an effect solely to justify a headline.

## Visual production

Use original QML shapes and prepared geometry for charts, maps, framing, and simple icons. Prefer appropriate existing freely licensed raster art for room scenes; custom project art fills gaps. Generating a bitmap is an option only when it solves a documented asset need and is consistent with the user's preference for existing suitable assets. No art generation is required for planning or M0.

Runtime raster images use PNG or JPEG. Vector map data and original QML geometry are data/code, not forbidden image codecs. Preserve editable sources when art is created for the project and include their provenance. No video is needed in 1.0. Avoid HEIC/HEVC, AVIF, JPEG XL/XS, and unreviewed media pipelines; do not make a worldwide patent-freedom promise about any format.

Technical illustrations clarify modeled relationships rather than pretend to reproduce a real benchmark. Historical hardware uses fictional names/appearance. State/site risk colors have symbols/text alternatives; map claims identify the source year and authored approximations. Natural Earth provides public-domain world boundaries; Census provides simplified domestic cartographic boundaries. The selected release, transformation and resulting geometry must be recorded. [Natural Earth terms](https://www.naturalearthdata.com/about/terms-of-use/), [Census boundaries](https://www.census.gov/geographies/mapping-files/time-series/geo/cartographic-boundary.html).

Possible low-cost UI source: Kenney's asset pages identify their game assets as CC0, subject to checking the actual pack/license. Do not use its logo as project branding. [Kenney source/license guidance](https://kenney.nl/support).

## Audio direction

Prefer ENDE's human-created catalog. Maintain a coherent, restrained ambient/electronic palette with low distraction during finance/research work. Tension tracks should signal a changed situation without making routine decisions feel like emergencies. Music can be muted independently; critical cues also appear as text/icons. No generated soundtrack and no mandatory voice acting.

Audio uses an optional playback adapter and silent fallback, without requiring the missing local Qt Multimedia module. Probe/pin miniaudio plus stb_vorbis before adopting it; disable unneeded decoders and restrict accepted input encodings. Use local Ogg Vorbis music only after decoder deployment is verified across the native matrix; short PCM WAV cues are the conservative effect format. Store original downloaded sources if redistribution permits, and record any encoding, edit, loop points and volume adjustment. A site license and a particular track's license are both checked; free to download is not a sufficient rights record. The ENDE standard license currently identifies CC BY 4.0 and game use. Keep attribution even where the site offers a waiver. [ENDE license](https://ende.app/en/standard-license).

## License and asset register

GPLv3 covers project code and original work designated that way. Public-domain/CC0, CC BY, OFL fonts, and third-party code licenses remain identified separately. Never set an asset's license to GPL merely because the repository is GPL. Avoid NC/ND, editorial-only, unknown-source, or otherwise incompatible material. Imported assets need a reviewed redistribution/use basis; if uncertain, use an original placeholder and record the unresolved request.

The [asset register](../assets/manifest.json) begins with the [prepared domestic map](../assets/maps/README.md). For every shipped asset it records:

| Field | Meaning |
| --- | --- |
| `id`, `path`, `sha256`, `kind` | Stable identity, relative shipped path, integrity, image/audio/font/data |
| `title`, `creator`, `source_url`, `retrieved_on` | Attribution and exact source; no search-result URLs |
| `license_id`, `license_url`, `license_evidence_path` | Actual applicable terms and locally retained evidence |
| `modifications`, `derived_from`, `purpose` | Edits, lineage, and in-game use |
| `ai_generated`, `generation_record` | Honest provenance, tool/date/prompt or local record where applicable |
| `credit_text`, `credit_group`, `review_status` | Offline credit display and clearance state |

Generated PNG/JPEG images additionally embed metadata stating AI origin, tool/date if known, and game purpose. Preserve a sidecar record because conversion can strip embedded metadata. Never invent authorship, exclusive copyright, a license grant from a generator, or a historical source. Generated assets still require provenance review; metadata is not legal clearance.

Music has a dedicated `assets/audio/CREDITS.json` view of title/artist/license/source/edits, derived from or checked against the register. Offline credits show required attribution and license text notices, plus Qt/third-party notices and source access information. Missing provenance, missing credits, path mismatches, or a shipped unreviewed asset fail the release gate.

Fonts may initially use a system fallback without redistributing font files. A bundled font needs its own actual license; OFL permits bundling subject to its conditions, and modified fonts may need renamed Reserved Font Names. [Official OFL guidance](https://openfontlicense.org/).

## Localization and review

English is the first shipped language. All UI/event strings use stable keys and support pluralization, units, currencies, and dates; no string concatenation that prevents translation. Do not claim translated language support before the content is reviewed. Accessibility descriptions are localization content too.

Review each batch for factual teaching accuracy, causal clarity, content tone, non-stereotyped characters, distinct choices, length in the actual UI, and license completeness. A successful JSON parse is necessary but does not establish useful writing, a lawful asset, or a readable screen.
