# Development roadmap

Baseline 1.1 · 2026-11-03 · Gate-driven plan, not a promised calendar

## Delivery strategy

Prove distribution/version identity first, establish the shared content runtime and a small usable authoring Studio, then prove one satisfying business loop and expand systems/the century. The full design is the 1.0 destination. It is not the first coding assignment.

**First playable:** a 2014–2017 specialist-company scenario with a complete tutorial, research/data/training/evaluation loop, contracts, cash pressure, two rivals, a lightweight incumbent warning, save/load, a recoverable setback, and a scored ending. Three states, one product channel, six research nodes, four data offers, eight authored events, Office/Lab plus management panels. All art may be original procedural placeholders with valid provenance. No full Congress, global domination, owned power plants, or century of content is required for this milestone.

**Full 1.0:** 2014–2114 and endless sandbox; all six objective families; 51 domestic jurisdictions; eight international market regions; all core systems in the design; qualified macOS arm64, Linux amd64/arm64, Windows amd64/arm64 native packages; entirely offline play. Snap is a separate post-primary-release milestone. Browser export and counties are future options, not hidden 1.0 dependencies.

**Separate authoring product:** [Content Studio](CONTENT_EDITOR.md), downloadable independently with source in this repository. E1 delivers repository browsing, typed trigger/effect/event graphs, all lifecycle templates, dialogue/results, inspection/test tools and versioned content finalization for the slice. E2 expands scenario/world/research/catalog/asset tools and qualifies five editor artifacts before bulk content work. Editor binaries/drafts/layouts never enter the base game. [Version tracking](VERSIONING.md) starts with one metadata record and later feeds Snap packaging.

## Milestones and acceptance gates

Effort ranges are initial planning estimates for one experienced full-time maintainer with coding-agent assistance and periodic art/content/playtest help. They include engineering and content integration, not an assumption that agents provide free infinite throughput. Hardware access and human feedback can add elapsed waiting time. Re-estimate after M0 and M3.

| Milestone | Effort | Dependency | Deliverable and gate |
| --- | --- | --- | --- |
| M0 — Toolchain and platform proof | 1–2 person-weeks | Planning baseline | Minimal headless target and QML window; Release-default wrapper; shared version reader/CI checks; five native CI compile/package probes; local-resource/audio test; dependency/license inventory. Record graphics/device gaps honestly. No large local SDK downloads. |
| M1 — Foundation | 3–5 | M0 | Deterministic units/time/PRNG; typed content/fact/effect registry and bounded flow compiler/runtime; transactional commands/snapshots; local save/autosave/preferences including flow state; accessible room shell; repeatable tests and inspection traces. A save resumes the same next-day state hash. |
| M2 — Headless business loop | 3–5 | M1 | Balanced accounts, finite capacity, contractual demand, cash flows, forecasts, warning/workout path, and a three-year scenario runner. No double spending, duplicated compute, or money creation. |
| E1 — Minimum useful Content Studio | 4–7 | M2 + UI shell | Separate editor; current-content library; typed trigger → effects/events canvas and equivalent outline/forms; news/dialogue/hidden flows and five lifecycle controls; truth/path/target/before-after inspection; branch/result preview and safe versioned repository writes. Three authors complete a small flow task and catch planted sign/scope mistakes without raw scripting. |
| M3 — First playable vertical slice | 4–6 | M2 + E1 | Data → research → training → held-out evaluation → deployment → contract → consequence; 30–45-minute scenario; eight Studio-authored/validated events; recovery and ending. At least five independent playtest sessions, with three players completing without facilitator intervention. |
| M4 — Competitors and corporate governance | 4–6 | M3 gate | Budgeted rival plans, Code Red escalation, financing/cap table, negotiation, executives, board decisions. Specialist and frontier approaches are both credible; opponents obey shared rules. |
| M5 — Geography, infrastructure, global markets | 4–7 | M4 | All states/DC, eight world profiles, hardware delivery/power/cooling/maintenance, owned vs leased capacity, six channels, disaster mitigation and insurance. No online map dependency; a map/list strategy comparison changes a real decision. |
| E2 — Expanded authoring and editor qualification | 3–5 | Starts after M5 + E1; closes after M7 | Scenario/world/policy and research/catalog/asset tools; map/target impact views, reusable flows/refactors, version/migration review; current-runtime preview parity; five clean native editor artifacts prepared as separate downloads. Finish before the M8 bulk content pass. |
| M6 — Politics and social response | 5–8 | M5 | US policy/election loop, international standards and market access, media/advertising, macro effects, executive/public disputes. Actions have delayed, explainable effects; ordinary ad spend cannot buy political victory. |
| M7 — Century, succession, and endgames | 5–8 | M6 | Branching future research, game AGI/ASI validation, bounded research automation, succession, six objectives with sustained accounting, warned catastrophes, campaign postmortem, sandbox continuation. Every objective has a demonstrated reachable fixture and a near-miss fixture. |
| M8 — Content, balance, accessibility, endurance | 5–8 | M7 + E2 | Minimum useful content coverage authored/managed in Studio, final art/audio provenance, viable strategy suite, full human campaign, 100-year headless batches and 500-year sandbox checks, performance measurements, accessibility and privacy evidence. Fix repetition and exploits before adding scope. |
| M9 — Native qualification and itch release | 3–5 | M8 + required device access | Five clean-device artifacts, source/license/credits/checksums, downloaded-package tests, offline/privacy audit, store copy and instructions, explicit release authorization, then upload. No platform ticked as supported on compile success alone. |
| M10 — Snap distribution | Separate estimate after M9 | Primary release complete | Ubuntu-based amd64/arm64 game packages, shared-manifest game version through Snapcraft metadata adoption, content/save migration and refresh tests, confinement/local saves/audio/graphics verification, store metadata and explicit submission task. No editor bundled and no new backend. |

Through M3, including E1, total **15–25 person-weeks before contingency**. Through M9, including E1/E2, total **44–72**, with a 25% uncertainty reserve giving approximately **55–90 person-weeks** (roughly 13–21 months of full-time effort). This adds approximately 8–14 person-weeks to the original engineering/content estimate; the benefit to authoring speed and error reduction must be demonstrated, not assumed. E2 work can be interleaved with M6/M7 but is not free parallel effort. Re-estimate after the first Studio usability test. A complex simulation with agent assistance still requires design judgment, playtesting, integration, and maintenance; do not convert this table into a promised launch date.

```mermaid
flowchart LR
    P[Planning baseline] --> M0[Platform proof]
    M0 --> M1[Foundation]
    M1 --> M2[Headless business]
    M2 --> E1[Minimum authoring Studio]
    E1 --> M3[Playable slice]
    M3 --> M4[Rivals and governance]
    M4 --> M5[Geography and global markets]
    M5 --> M6[Politics and society]
    M5 --> E2[Expanded Studio and editor qualification]
    M6 --> M7[Century and endings]
    M7 --> E2
    E2 --> M8[Balance and endurance]
    M8 --> M9[Five native itch packages]
    M9 --> M10[Snap]
```

## Scope budgets

The first 1.0 content targets are 60 research nodes across seven tracks, 24 data-source offer families, 18 hardware families, 12 product templates across six channels, 120 authored event definitions, 24 executive profile templates, seven main location sets plus four contextual sets, and roughly 60 reusable icons. These are coverage budgets, not a license to pad content. The core should remain interesting with fewer entries; a larger catalog cannot repair a broken loop.

The standard world has 15 rival institutions scheduled in total (6 domestic, 6 international, 3 heavyweights). Test configurations up to 32 total AI institutions. Raise bounds only after performance measurement. Named executive templates generate varied fictional people locally; no 100-year roster must be handcrafted character by character.

## Stop, fix, or cut rules

- If a native toolchain/renderer cannot be packaged in M0, resolve it or document an engine/platform adjustment before building more UI. Do not discard an architecture just because one untested runner fails once.
- If new players cannot explain cash burn, training tradeoffs, and their first contract after the slice, revise feedback and pacing before adding politics.
- If only one strategy survives, fix economics/constraints before expanding research content.
- If late years repeat the same low-value action, add policies and summaries; do not force a hundred years of manual renewals.
- If content effort exceeds capacity, reduce repeated events, decorative sets, hardware variants, minor offer types and optional editor conveniences first. Preserve the six objectives, their necessary mechanisms, usable graphical authoring/impact inspection, offline privacy, honest platform claims, and the full campaign premise. A reduction to promised core scope needs an explicit decision record.
- Keep E1 focused on the slice and an extensible typed registry. Full world maps, elaborate layout styling and every late-game specialty form are E2 work; a small useful editor must precede a universal authoring framework.
- Do not trade reliable saves, clear causality, keyboard access, or privacy verification for more release content.

## Execution and checkpoints

Use [the backlog](BACKLOG.md) as the assignment queue. One task should fit one coherent review and normally one focused agent session; split work when it cannot. Record tests/evidence, any changed assumptions, and the next task in [status](STATUS.md). Commit and push completed task work to preserve it. A milestone closes only when its gate evidence exists, not when its files have been created.

Every milestone ends with a short playable or headless demonstration and a decision: proceed, repair, or explicitly revise scope. No release upload, store setup, payment, or account change is implied by this roadmap. Those are concrete later tasks with their own access requirements in [needs and access](NEEDS_AND_ACCESS.md).
