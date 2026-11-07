# Project status and next handoff

Updated 2026-11-07

## Implementation progress

**The initial skeleton phase is complete and closed for human feedback.**

The user accepted the current core as the starting point for the next phase
and directed the initial implementation pass to wrap up. The gameplay
checkpoint is `d1f2bd3`, with finalized internal content
**0.29.0-dev.1**, generation **29**.

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

At the user's direction, New Game retains the existing prototype setup.
The gameplay interface, starting flow, technology direction, isometric/3D
environments and a usable Mac Content Studio remain subsequent feedback work.

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

The Mac game target produces an application bundle. `./make.sh --package`
stages a self-contained, ad-hoc signed development app in `dist`; the Codex
Run action explicitly builds/packages/launches through
`script/build_and_run.sh`. Builds alone never launch it. Native menu rendering
was inspected; the completed offscreen keyboard check covers 200% text,
persisted preferences, audio decoding, the system AVFoundation video loop,
prototype entry/resume, load/error recovery and confirmed/cancelled exit.
The basic campaign clock/save/load walkthrough also passes.

Ten of eleven focused CTests passed. The unchanged campaign-difficulty path
fails its forecast-padding assertion, and the older full-campaign GUI check
fails its staffing assertion after the current founder setup. Those core/test
issues are recorded for the next selected refinement, not hidden by the menu
checks. Native file-picker row selection, VoiceOver and release/device
qualification remain open. Content Studio usability is not accepted by this
milestone.

At closure, the shared content check verifies generation 29, and the build
graph reports no work for the game or editor targets. Game and editor versions
remain development placeholders, and release readiness is false.

## Next phase

1. Gather and review human feedback and the user's work-in-progress notes.
2. Select concrete changes and refine the core around that feedback.
3. Expand events after those changes establish the direction.
4. Balance and undertake extended qualification afterward.

`tech_sug.txt` is a work-in-progress reference for later review. Its ideas
have not been adopted as implementation requirements.

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
