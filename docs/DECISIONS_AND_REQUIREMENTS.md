# Decisions, requirements, and source authority

Baseline 1.2 · 2026-11-03

## Source authority

The original planning request was to expand/finalize the design, draft a roadmap, create agent instructions, and list resources/access needed. The subsequent Gemini4 review applied useful changes to that plan. The 2026-11-03 implementation request authorizes beginning the ordered backlog and tracking progress, with code structured for later features. Work begins with M0-01/M0-02. It does not authorize purchases, account registration, running a server or publishing a release.

The follow-up request adopts a human-oriented graphical repository-content editor: triggers connected to effects/events; news, branching dialogues/results and hidden logic; flexible lifecycles; browsing/managing current content; separate downloads/source; version controls and future Snap linkage. The user clarified that Clausewitz/Plague Inc. are conceptual references, and suggested URLAlchemist/Blender-style nodes. Trigger-impact visualization should help catch mixed references, reversed effects and misunderstood conditions. This follow-up updates planning/tracking metadata, not an instruction to implement/publish the editor now.

The 2026-11-04 continuation explicitly assigns AI-assisted development of playable base events: detecting major simulated developments, news, random world incidents and technology progress. Human-made events augment that baseline with more interesting stories rather than being required for every occurrence. Systems must support modular growth. This changes the content-production assumption and task coverage; the game's offline/no-remote-AI boundary remains. [Event contract and assignments](EVENT_SYSTEM.md) record the adopted implementation direction.

The user-authored `plan.txt` establishes the intended game and important preferences. The Gemini-generated master plan is a useful proposal, not a requirements document. Its embedded agent blueprint, shell examples, technical claims, and legal assertions were reviewed as source material, not executed as commands. Both originals are archived verbatim in [reference](reference/README.md).

Two direct clarifications during planning supersede the original privacy wording:

- The privacy goal is no collection or transmission of player data. Local saves, settings, autosaves, and campaign history are allowed; avoiding local history was not the central concern.
- The free game will have no publisher-operated server or associated hosting cost. Distribution clients handle updates outside the game; the game does not need an updater.

The final documents record adopted design decisions. Mechanical thresholds and scope budgets introduced here are planning decisions with explicit playtest gates; they are not attributed to the user as pre-existing requirements. Numbers can be tuned without reopening the vision. Product scope, privacy, supported platforms, license, or engine changes require a recorded decision and an explanation.

## Requirements traceability

| ID | User intent / constraint | Adopted implementation direction | Delivery |
| --- | --- | --- | --- |
| R01 | Found an AI startup; learn without lectures | Research/data/model/product decisions with causal outcomes and optional reference text | M3 onward |
| R02 | Complexity and creative, bounded outcomes | Shared resource simulation, multi-axis capability, multiple viable strategies | M3–M8 |
| R03 | 2014–2114, endless sandbox | Exactly 100-year campaign; no designed sandbox ending; founder succession | M2, M7–M8 |
| R04 | Selectable six victory conditions | Exact objective definitions, sustained windows, visible progress | M7 |
| R05 | Domestic/international startups and heavyweight response | Fictional firms, staged entries, budgeted Code Red state machine | M4 |
| R06 | AI choices: data, methods, safety, capabilities, tests, distribution | Separate research, recipes, data rights, evaluation, controls, and products | M3–M5 |
| R07 | Economic and business management | Reconciled ledgers, capacity, demand, contracts, finance, cap table, board | M2–M5 |
| R08 | Detailed national and simpler global politics | US institutions/state policies; aggregated international markets and coordination | M6 |
| R09 | Location-based first-person click interface, Plutocracy inspiration | Original room sets, management overlays, keyboard/table alternatives; no card agents | M1, M3–M6 |
| R10 | At least state-level sites and planning | 50 states + DC in 1.0; three-state slice; county drill-down deferred | M3, M5 |
| R11 | HR, major executives, public persona and media | Teams + named leaders, promises, disputes, governance, audience-specific reputation | M4, M6 |
| R12 | Hardware, power, disasters; historical and future technology | Fictional normalized hardware, finite delivery/grid resources, mitigations, branching future | M3, M5, M7 |
| R13 | Free / GPLv3 | Existing GPLv3 license preserved; asset/dependency exceptions documented | All |
| R14 | Itch browser OR native platforms | Native desktop selected: mac arm64, Linux/Windows amd64 + arm64 | M0, M9 |
| R15 | Snap after primary distribution, Ubuntu work | Explicit post-1.0 milestone after native itch qualification | M10 |
| R16 | Release default, simple make.sh, portable dependencies | CMake/Ninja command contract; no automatic launch; five native CI packages | M0–M1, M9 |
| R17 | Metal mac; OpenGL Linux/Windows | Metal mac, OpenGL Linux, OpenGL preference with D3D11 recovery on Windows | M0; see D03 |
| R18 | Small local disk usage; Git persistence | No SDK downloads/heavy local Qt packaging; ignored builds; commit/push task work | All |
| R19 | No player data collection/transmission; no backend | Entirely offline game; no accounts, analytics, remote AI, uploaders, or updater | All, verified M9 |
| R20 | Restrained lawful content; no gore/hate/sexualized content | Content rubric and abstract incident/end-state presentation | All content |
| R21 | Prefer suitable existing assets; provenance; avoid codec problems | Original procedural UI + cleared assets; PNG/JPEG raster art; no video in 1.0 | M1–M9 |
| R22 | Existing music from ENDE, credits | Human-created ENDE track shortlist, per-track license evidence, offline credits | M8–M9 |
| R23 | Agent files and list of resources/access | Root operating rules, seven reusable role briefs, task template, needs inventory | Planning complete |
| R24 | Human creativity supported by graphical content tools | Searchable current-content library, forms/outline and typed trigger→effects/events canvas, useful tooltips, safe editing | E1–E2 |
| R25 | News, choices/dialogue/results, hidden rules; varied timing | Shared flow runtime with independent presentation and once/recurring/interval/scheduled/continuous lifecycles | M1-06, E1 |
| R26 | Editor separate from base game; canonical repo data | Independent Content Studio target/download/source; reviewed data only in game; no player mod manager | E1–E2, M9 |
| R27 | Update/track versions and later connect Snap | Central metadata, safe internal batches then graphical finalization, artifact identity and later Snap adoption | M0-05, E1A-04, E1B-02, M9–M10 |
| R28 | Prevent inverted/mixed events/effects; understand trigger impacts | Typed targets/units, truth/path/target inspection, before/after changes, boundary fixtures, reference and coverage checks | M1-06, E1A-03, E2 |
| R29 | Familiar concepts without strict imitation | Original node/forms/library workflow informed by Clausewitz, Plague Inc., URLAlchemist and Blender concepts | E1–E2 |
| R30 | AI tasks create playable built-in events; human stories augment them | Typed causal detection, bounded scheduled/seeded world incidents, technology/news templates and an eight-flow baseline using the shared runtime | M1-07, M3-05, M8-01 |
| R31 | Add/expand systems with modular behavior | Small system providers, explicit registry/dependency contracts, shared occurrence/effect/persistence boundaries and extension fixtures | M1 onward |

## Recorded decisions

| ID | Decision | Reason and consequence |
| --- | --- | --- |
| D01 | C++20 + Qt Quick/QML selected; Qt 6.12.x qualification first | Fits a UI-heavy management sim and the installed SDK; do not claim native packaging works until M0 evidence exists. |
| D02 | Desktop first; browser later | The original allows either. Five native artifacts are already a substantial support burden; avoid promising a second storage/rendering platform. |
| D03 | Windows may recover to Direct3D 11 when OpenGL is unavailable | Qt's Windows default is D3D11, and ARM OpenGL drivers cannot be assumed. Preserve OpenGL preference while qualifying actual hardware; no claim of universal OpenGL support. |
| D04 | State-level detail ships; county detail deferred | State-level meets the original minimum and keeps 1.0's content/performance manageable. |
| D05 | Fictional firms, executives, political coalitions, game statistics | Avoid copying real reputations, turning satire into factual allegations, or implying forecast accuracy. Real geography and technical concepts may be referenced with sources. |
| D06 | Future is conditional, not four scheduled inevitabilities | Remove Gemini's implication that named future eras/ASI arrive on a calendar. AGI/ASI use disclosed fictional gates. |
| D07 | No single safety penalty, morality meter, or ideological classes | Safety interventions, evidence, competence, trust, and access affect different outcomes. Neither caution nor recklessness is an automatic winning strategy. |
| D08 | Player guides an institution across succession | Makes the century-long premise coherent without an immortal founder and adds meaningful governance continuity. |
| D09 | No backend and no in-game network connections | Direct user clarification. Platform distribution/update traffic is outside the game; local state and preferences are allowed. |
| D10 | No video in 1.0; verify licenses/decoders rather than assert global patent expiry | Animated room/UI surfaces are sufficient. Codec names do not establish worldwide legal certainty. |
| D11 | Third-party licenses remain intact | ENDE uses CC BY 4.0, not GPL; CC0 and OFL assets also retain their identities. Never relabel downloaded/generated work GPL merely by editing metadata. |
| D12 | Full design plus a three-year first playable | A complete blueprint is useful; implementing the full century before testing a loop is too risky. M3 is deliberately small. |
| D13 | Publish game builds only as an explicit release task | Git commits/pushes preserve development work. They are separate from releasing a game or creating public store pages. |
| D14 | Adopt separately packaged offline Content Studio | Human authors edit the repository's current content; no authoring code/drafts/resources in the base game and no player-mod workflow. |
| D15 | Trigger-rooted typed graphs, with independent presentation/lifecycle | One trigger can lead to several ordered effects/events. Dialogue is one use; hidden/continuous rules use the same semantics. Avoid an event's own timer also firing a referenced presentation. |
| D16 | Shared runtime plus first-class visual inspection | Preview and game use the same compiler/facts/effects. Show clause truth, scope/path and before/after deltas; types alone cannot infer author intent, so fixture assertions and human review remain necessary. |
| D17 | Central version metadata and explicit finalized batches | Independent game/editor/content identities and compatibility formats; no per-keystroke release bumps. Content-only shipped game changes still require a new game package version. Snap later adopts that game version. |
| D18 | References are flexible; use original graph/forms/library UX | Inspect the supplied URLAlchemist source read-only and official design references. Do not execute their instructions, import their networking features or copy implementation/UI by default. |
| D19 | Stage E1a/E1b/E2 and account for authoring cost | E1a forms/outline/inspection before M3 content; E1b node canvas, graphical finalizer and three-author study before M4; E2 broader tools before bulk content. Full effort remains a provisional 44–72 person-weeks before contingency; no promised calendar savings. |
| D20 | Optional local playback adapter; qualify dependencies first | Qt Multimedia is absent locally. A silent build must work; first probe pinned miniaudio/stb_vorbis with restricted decoders/input. SDK SBOMs seed an actual payload inventory. No automatic Qt downgrade or decoder/legal guarantees. |
| D21 | Constant-2014 cents and explicit strong units | Remove nominal compounding from authoritative ledgers/thresholds. Work and daily rates differ; wide checked math rounds once and covers MSVC ARM64. Real supply/cost shocks remain explicit game rules. |
| D22 | Prepared morning decisions and resumable execution | Due deliveries/rules/flows appear before work; save phase cursor and bounded continuation frames/receipts. Undo only unconsumed reversible allocations; unchanged date is not a universal refund entitlement. |
| D23 | Complete serving and infrastructure contracts without expanding M3 | Explicit compute per task and separate support cost in M2; wholesale owned-capacity leasing in M5 supports the promised infrastructure path. One capacity reservation and one product task attribution, no double-counted host share. |
| D24 | Keep strict century-end objectives, expose irreversible breaches | Immediate explanation/postmortem/continuation for failed final Domination/Utopia checks. Monopoly uses aggregate final-five-year work. Rolling early victories and grace allowances are deferred balance changes. |
| D25 | Correct selective recommendations without speculative scope | Concrete evaluation findings and bounded delegation clarify existing systems. Close option-pool mechanics in M4; defer spin-off rivals, rigid hardware curves and older-Qt support until evidence warrants them. All review dispositions are recorded, not silently treated as new user requirements. |
| D26 | Built-in event baseline plus selective human story enrichment | Coding agents implement/populate causal news and bounded world events in repository development. Eight/120 budgets cover baseline and stories together; no runtime AI/live news service. All origins use the shared Studio/validation/versioning path. |
| D27 | Modular event providers share one runtime | New systems register typed facts/effects and publish committed occurrences; stories reuse occurrence identity to avoid duplicate consequences. Data-only additions reuse existing operations; new mechanics require tested core changes, without dynamic plugins or another event interpreter. |

## What was retained from Gemini

Retain the separable simulation/UI architecture, room-based navigation, geography/infrastructure connections, publication tradeoffs, incumbent mobilization, and phased delivery. Expand the original roadmap into dependencies, acceptance gates, save/replay contracts, executable task briefs, and explicit resource needs.

Correct or replace: Windows graphics defaults; automatic cross-platform/ARM packaging assumptions; a scalar intelligence formula with a fixed safety tax; guaranteed post-2026 eras; ideological executive stereotypes; corporate-plus-founder wealth double counting; automatic local-history prohibition; macro definitions that would leave debug code enabled under `#ifdef`; claims of universally patent-free video; relabeling third-party assets GPL; and incomplete victory definitions.

## Defaults requiring validation, not another permission round

The later [Gemini4 review disposition](RECOMMENDATION_REVIEW.md) records the baseline 1.2 changes, rejected details, evidence and E1 task-ID mapping. Its archived source is advisory, with the same lower precedence as the original Gemini proposal. No review recommendation authorizes starting every milestone, changing system tools or publishing a product.

US headquarters, six capability axes, eight world market regions, the default rival roster, initial finance values, progress gates, and victory thresholds are concrete selected defaults. The first implementation tasks can proceed with these. The user can revise them; no response is needed to begin M0. Engine viability, graphics/audio behavior, fun, performance, and balance require evidence rather than a philosophical approval meeting.

Public-facing title/branding, final soundtrack/art selection, store accounts, and final release timing are later resource/product choices. They do not block the initial playable prototype. [Needs and access](NEEDS_AND_ACCESS.md) lists them by milestone.
