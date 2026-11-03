# Development roadmap

Baseline 1.0 · 2026-11-03 · Gate-driven plan, not a promised calendar

## Delivery strategy

Prove distribution first, prove one satisfying business loop second, then expand systems and the century. The full design is the 1.0 destination. It is not the first coding assignment.

**First playable:** a 2014–2017 specialist-company scenario with a complete tutorial, research/data/training/evaluation loop, contracts, cash pressure, two rivals, a lightweight incumbent warning, save/load, a recoverable setback, and a scored ending. Three states, one product channel, six research nodes, four data offers, eight authored events, Office/Lab plus management panels. All art may be original procedural placeholders with valid provenance. No full Congress, global domination, owned power plants, or century of content is required for this milestone.

**Full 1.0:** 2014–2114 and endless sandbox; all six objective families; 51 domestic jurisdictions; eight international market regions; all core systems in the design; qualified macOS arm64, Linux amd64/arm64, Windows amd64/arm64 native packages; entirely offline play. Snap is a separate post-primary-release milestone. Browser export and counties are future options, not hidden 1.0 dependencies.

## Milestones and acceptance gates

Effort ranges are initial planning estimates for one experienced full-time maintainer with coding-agent assistance and periodic art/content/playtest help. They include engineering and content integration, not an assumption that agents provide free infinite throughput. Hardware access and human feedback can add elapsed waiting time. Re-estimate after M0 and M3.

| Milestone | Effort | Dependency | Deliverable and gate |
| --- | --- | --- | --- |
| M0 — Toolchain and platform proof | 1–2 person-weeks | Planning baseline | Minimal headless target and QML window; Release-default wrapper; five native CI compile/package probes; local-resource/audio test; dependency/license inventory. Record graphics/device gaps honestly. No large local SDK downloads. |
| M1 — Foundation | 2–3 | M0 | Deterministic units/time/PRNG; validated content; transactional commands and snapshots; local save/autosave/preferences; accessible room shell; repeatable tests. A save resumes the same next-day state hash. |
| M2 — Headless business loop | 3–5 | M1 | Balanced accounts, finite capacity, contractual demand, cash flows, forecasts, warning/workout path, and a three-year scenario runner. No double spending, duplicated compute, or money creation. |
| M3 — First playable vertical slice | 4–6 | M2 + UI shell | Data → research → training → held-out evaluation → deployment → contract → consequence; 30–45-minute scenario; eight events; recovery and ending. At least five independent playtest sessions, with three players completing without facilitator intervention. |
| M4 — Competitors and corporate governance | 4–6 | M3 gate | Budgeted rival plans, Code Red escalation, financing/cap table, negotiation, executives, board decisions. Specialist and frontier approaches are both credible; opponents obey shared rules. |
| M5 — Geography, infrastructure, global markets | 4–7 | M4 | All states/DC, eight world profiles, hardware delivery/power/cooling/maintenance, owned vs leased capacity, six channels, disaster mitigation and insurance. No online map dependency; a map/list strategy comparison changes a real decision. |
| M6 — Politics and social response | 5–8 | M5 | US policy/election loop, international standards and market access, media/advertising, macro effects, executive/public disputes. Actions have delayed, explainable effects; ordinary ad spend cannot buy political victory. |
| M7 — Century, succession, and endgames | 5–8 | M6 | Branching future research, game AGI/ASI validation, bounded research automation, succession, six objectives with sustained accounting, warned catastrophes, campaign postmortem, sandbox continuation. Every objective has a demonstrated reachable fixture and a near-miss fixture. |
| M8 — Content, balance, accessibility, endurance | 5–8 | M7 | Minimum useful content coverage, final art/audio provenance, viable strategy suite, full human campaign, 100-year headless batches and 500-year sandbox checks, performance measurements, accessibility and privacy evidence. Fix repetition and exploits before adding scope. |
| M9 — Native qualification and itch release | 3–5 | M8 + required device access | Five clean-device artifacts, source/license/credits/checksums, downloaded-package tests, offline/privacy audit, store copy and instructions, explicit release authorization, then upload. No platform ticked as supported on compile success alone. |
| M10 — Snap distribution | Separate estimate after M9 | Primary release complete | Ubuntu-based amd64/arm64 packages, confinement/local saves/audio/graphics verification, store metadata and explicit submission task. No new backend. |

M0–M3 total **10–16 person-weeks before contingency**. M0–M9 total **36–58**, with a 25% uncertainty reserve giving approximately **45–75 person-weeks** (roughly 10–18 months of full-time effort). These ranges are provisional. A complex simulation with agent assistance still requires design judgment, playtesting, integration, and maintenance; do not convert this table into a promised launch date.

```mermaid
flowchart LR
    P[Planning baseline] --> M0[Platform proof]
    M0 --> M1[Foundation]
    M1 --> M2[Headless business]
    M2 --> M3[Playable slice]
    M3 --> M4[Rivals and governance]
    M4 --> M5[Geography and global markets]
    M5 --> M6[Politics and society]
    M6 --> M7[Century and endings]
    M7 --> M8[Balance and endurance]
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
- If content effort exceeds capacity, reduce repeated events, decorative sets, hardware variants, and minor offer types first. Preserve the six objectives, their necessary mechanisms, offline privacy, honest platform claims, and the full campaign premise. A reduction to promised core scope needs an explicit decision record.
- Do not trade reliable saves, clear causality, keyboard access, or privacy verification for more release content.

## Execution and checkpoints

Use [the backlog](BACKLOG.md) as the assignment queue. One task should fit one coherent review and normally one focused agent session; split work when it cannot. Record tests/evidence, any changed assumptions, and the next task in [status](STATUS.md). Commit and push completed task work to preserve it. A milestone closes only when its gate evidence exists, not when its files have been created.

Every milestone ends with a short playable or headless demonstration and a decision: proceed, repair, or explicitly revise scope. No release upload, store setup, payment, or account change is implied by this roadmap. Those are concrete later tasks with their own access requirements in [needs and access](NEEDS_AND_ACCESS.md).
