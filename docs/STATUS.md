# Project status and next handoff

Updated 2026-11-07

## Implementation progress

**The initial skeleton phase is complete and closed for human feedback.**

The user accepted the current core as the starting point for the next phase
and directed the initial implementation pass to wrap up. The gameplay
checkpoint was `d1f2bd3`, with internal content 0.29.0-dev.1. The current
world/interface batch advances content to **0.32.0-dev.1**, generation **32**.

The detailed game design remains a reference for feedback and future work.
Working-condition programs and other deeper mechanics previously proposed as
next tasks are candidates for that review. The initial implementation queue
is closed.

## Selected feedback: Mac application and main menu

The first human review identified missing Mac application packaging, a
confusing game interface and an unusable Content Studio entry point. The
selected first milestone supplies **Project Glowing Pancake.app**, its icon,
bundled resources/Qt, and a separate full-screen main menu: New Game, Load
Game, Settings, Credits and Quit. Settings saves display mode, window size,
audio, text size, reduced motion and still/video background choices locally.
Escape opens an exit confirmation with Cancel and Exit game, including during
play. The Main Menu button pauses; Resume returns to the same session.

New Game now uses the founder setup described below.
The gameplay interface foundation is described below; usable Mac Content Studio remains open.

## Selected feedback: inhabitable company and portraits

The game now opens a continuing company automatically, with an original 3D
workspace, permanent HUD, device applications and HQ/city/state/country/world
navigation. Escape pauses into the menu. Accepted actions and completed days
persist atomically; storage failure stops play and preserves recovery files.
Unfinished named form fields and the current navigation are stored locally.

HQ recruitment supplies stable generated profiles, real signing costs/payroll,
onboarding and usable evaluation expertise. Branch operators remain aggregated.
The founder campaign adds 3,143 counties with a 2010 Census population/location
baseline and explicitly fictional bounded monthly economic trends. World outlines
and region briefings are offline. Employee portraits combine versioned vector
part/color IDs saved in each profile; selecting a portrait opens its dossier.

Focused core/persistence checks and native navigation/restart checks pass.
Portrait layer rendering, 1080p/8K offscreen viewports and a narrow 200% text
profile are checked separately from attached-display native Metal evidence.
Paid HQ rooms reserve real workstation capacity and open after construction.
Reusable facility blueprints buy state-specific physical sites, hardware and
operator teams through one transaction; inspection and commissioning remain
required. The [selected design](GAMEPLAY_INTERFACE.md) records remaining device
lifecycle, manager effects, briefing progression and retirement narrative work. These checks
do not close human review, extended campaign or release/device gates.

## Selected feedback: campaign and technology design

F-02 documents a new campaign direction: a California birth in 1997, formative
choices and four leg ups, founding at 18 in 2015, company/industry stages,
mission-linked objectives, distinct investor/donor/nonprofit terms, and an
age-118 ending in 2115. The completed history/result stays frozen if the player
continues in a post-game sandbox.

Both supplied technology PDFs were reviewed as advisory sources. The new plan
separates knowledge, implementation, rights and evidence; covers research
variants, publication/prestige, licensing and partnerships; and provides 140
candidate entries across ten domains plus ten bounded refinement families.
Unfounded safety guarantees, runaway compounding and a mandatory cosmic climax
were rejected or deferred. [Campaign flow](CAMPAIGN_FLOW.md) ·
[Technology](TECHNOLOGY.md) · [Candidate catalog](TECHNOLOGY_CATALOG.md).

New Game opens on the hospital prologue. Each beat uses original silhouette art,
staged narration and one decision on a proportional canvas. Family background
and legal form change later illustrations and accents. Identity, traits and eight
formative choices lead into the entry route, legal form, mission, funding
offers, office and first research assignment. The signing review reconciles
contributions, costs, monthly commitments and ownership. The complete biography
persists in campaign saves. The company starts paused in 2015.

Standalone sandbox creation was removed. The save reader still supports prior
saves. Later stages, ending presentation and the first-play tutorial remain
in the backlog. [Images and pacing](../assets/founding/README.md).

## Implemented core

| Area | Available starting point |
| --- | --- |
| Campaign and persistence | Company setup, difficulty and seed choices, paused calendar, local saves, autosaves, exact continuation and a playable 2014–2017 campaign |
| Lab | Licensed, commissioned, feedback and synthetic data, research and setbacks, training and checkpoints, independent evaluation, deployment and specialist assignment |
| Business and people | Accounts, contracts, forecasts, taxes, funding, protected budgets, paid hiring and onboarding, staffing pressure, executives and board decisions |
| Products and competitors | Product channels, staged releases and rollback, compatibility and retirement, enrollment, incident response, rival plans and heavyweight responses |
| Geography and infrastructure | Domestic map, international regions, owned and leased compute, power, delivery, resilience, insurance and wholesale contracts |
| Society and long campaigns | Civic policy, elections, regional response, institutional cases, conditional research, succession, objectives, postmortems and sandbox continuation |
| Content and authoring | Shared event runtime, built-in news and optional stories, separate Content Studio, typed graph/forms, inspection and versioned content snapshots |

The [playability guide](PLAYABILITY.md) describes controls and playable paths.
The [implementation log](IMPLEMENTATION_LOG.md) records the behavior and
focused engineering checks.

## Current development build

Plain `./make.sh` builds both applications in Release with local CPU tuning,
`BUILD_TESTING=OFF` and developer diagnostics disabled. It runs no tests and
resets a cache previously configured with tests. `--enable-tests` explicitly
includes test targets/GUI checks; `--test` also runs tests. `--enable-debugging`
selects Debug, and `--portable` disables host CPU tuning. Incremental caches
are retained. The Codex Run action builds/packages/launches only the game via
`script/build_and_run.sh`; builds alone never launch an application.

Focused setup checks cover New Game, one question at a time, resizing to
650×480 at 100% and 200% text, family and legal-form artwork, funding
eligibility, the paused first day and exact reload. The native Mac prologue
walkthrough and offscreen main menu check pass. The separate native menu
check ended during its full-screen restore. Core checks cover alternative
founding routes and prior save compatibility. Build-wrapper checks cover both-app defaults and explicit
opt-ins. No full suite, long campaign or native matrix is part of this batch.
Native file-picker row selection, VoiceOver and release/device qualification
remain open. The prior forecast-padding and full-campaign staffing assertions
remain recorded for later selected work. Content Studio usability is not
accepted by this milestone.

Finalized content is generation 30. Game and editor versions remain development
placeholders, and release readiness is false.

## Next phase

1. Gather and review human feedback and the user's work-in-progress notes.
2. Select concrete changes and refine the core around that feedback.
3. Expand events after those changes establish the direction.
4. Balance and undertake extended qualification afterward.

`tech_sug.txt` remains an unchanged work-in-progress reference. The selected
technology design above records what is now adopted as planning direction;
other ideas still need review/selection before implementation.

The [backlog](BACKLOG.md) carries the deferred human, QA, packaging and
publication gates with their original IDs. Their scheduling follows the
user's next phase instructions.

## Initial feature coverage

The original 52-step tracker is archived at **42 recorded implementation
acceptances and ten deferred later gates**. Its unchecked items retain their
original criteria. Closing the skeleton phase records the user's handoff
decision.

- [Original checklist](archive/initial-implementation-backlog.md)
- [Implementation coverage and handoffs](archive/initial-implementation-status.md)
- [Original milestone roadmap](archive/initial-roadmap.md)
