# Feedback and later-phase backlog

Updated 2026-11-07

**The initial skeleton implementation queue is closed.** The current core is
the starting point for human feedback. Work now follows the user's selected
feedback changes. Proposed deeper mechanics from the initial pass await
that review.

## Next phase

Selected feedback milestone **F-01 — Mac game app and main menu** is complete:
full-screen entry, New/Load, settings, credits, image/video background, music
and effects, and Escape/quit confirmation. New Game deliberately retains the
existing prototype setup. The game has a local `.app` with an icon and bundled
dependencies; this does not close the later device/release qualification gates.

Selected feedback **F-02 — campaign flow and technology design** is documented
in [Campaign flow](CAMPAIGN_FLOW.md), [Technology](TECHNOLOGY.md) and the
[140-candidate catalog](TECHNOLOGY_CATALOG.md). It adopts the 1997 introduction,
2015–2115 founder campaign, conditional stages, mission/backer/legal-form setup
and frozen ending with optional sandbox. This completes the design revision,
not runtime implementation. The PDF proposals remain advisory and reviewed.

The next discussion is the main-game layout and its changes by stage. Runtime
implementation of F-02, environment art and the user's Content Studio
usability/application feedback remain open; no broad implementation starts from
this documentation task. The existing forecast-padding
CTest and full-campaign staffing assertion also need review in a later
selected core/flow pass.

| Order | Work | Starting condition |
| --- | --- | --- |
| 1 | Gather human feedback and review the user's notes | The user is ready to begin feedback |
| 2 | Refine and extend the core | Concrete feedback changes have been selected |
| 3 | Substantial event expansion | The feedback changes establish the direction |
| 4 | Balance and extended testing | Core refinements and event expansion are in place |
| 5 | Native qualification and release | A separate release preparation request and required access |

The reviewed technology direction supersedes the earlier blanket deferral of
that topic. `tech_sug.txt` remains an unchanged work-in-progress reference;
unselected details do not become implementation tasks automatically.

**Deferred TODO:** when gameplay/layout are finalized, create the final tutorial,
offer it on first play, allow Skip/replay and verify the actual founding routes.

## Carried acceptance gates

The original criteria remain in the [archived 52-step checklist](archive/initial-implementation-backlog.md).
These ten gates belong to the phases above.

| Original ID | Remaining acceptance | Phase |
| --- | --- | --- |
| M3-04 | Independent player sessions | Human feedback when ready |
| E1B-03 | Human author usability study and associated acceptance | Human feedback when ready |
| M8-02 | Balance, full campaigns and endurance | After feedback and event expansion |
| M8-03 | Performance, privacy and dependency audit | Extended qualification |
| M0-04 | Five native CI package probes | Release preparation |
| E2-04 | Separate editor downloads and qualification | Release preparation |
| M9-01 | Five clean-device artifacts | Release qualification |
| M9-02 | Reviewable itch release package | Release preparation |
| M9-03 | Publish the approved primary release | Explicit publication task |
| M10-01 | Snap packages, refresh and confinement qualification | After the primary release, under an explicit task |

## Completed initial phase

The original checklist records 42 implementation acceptances. A subsequent
coverage pass delivered 25 core feature batches through gameplay commit
`d1f2bd3`. Those batch notes are historical implementation records.

- [Closed-phase status and coverage](archive/initial-implementation-status.md)
- [Original checklist and acceptance criteria](archive/initial-implementation-backlog.md)
- [Implementation evidence](IMPLEMENTATION_LOG.md)
- [Current handoff](STATUS.md)
