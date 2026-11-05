# Agent task guide

This directory contains reusable role briefs for human-directed or agent-assisted implementation. `AGENTS.md` is the automatic repository instruction file. These Markdown role files do not create processes, register custom agents, select a model, grant account access, or authorize delegation.

## Choosing a role

| Role brief | Use for |
| --- | --- |
| [Lead/integration](roles/lead.md) | Scope, design consistency, sequencing, cross-system integration and handoffs |
| [Simulation](roles/simulation.md) | Core mechanics, data validation, accounts, deterministic behavior and persistence |
| [UI](roles/ui.md) | QML, room navigation, bridge models, accessibility and player feedback |
| [Content/assets](roles/content.md) | Research/event data, writing, art/audio sourcing, licensing and credits |
| [Content editor](roles/content-editor.md) | Separate graphical Studio, typed node flows, trigger/effect inspection, repository writes and version controls |
| [QA/balance](roles/qa.md) | Behavioral tests, scenarios, exploits, playtesting, endurance and acceptance evidence |
| [Build/release](roles/build-release.md) | CMake, make.sh, CI, deployment, source/license bundles and authorized publication |

One agent can use multiple briefs where a task crosses boundaries. Parallel agents are optional only when explicitly authorized; if used, assign non-overlapping file ownership and integrate through shared contracts. A role brief does not lower repository privacy/content/authorization rules.

## Ready-to-use starting assignment

> Complete the M3 First Playable slice (`M3-01`, `M3-05`, `M3-02`, `M3-03`) in `docs/BACKLOG.md` following `AGENTS.md`. Prioritize shipping a complete, playable 2012–2020 base game loop on macOS (`M3-01` data/research/training/evaluation loop -> `M3-05` eight base events and bounded rivals -> `M3-02` playable Office/Lab UI panels, HQ choice, training preview, contract comparison, and tutorial -> `M3-03` delegation, warnings, recovery, and end-of-slice scoring). Do not block `M3` gameplay implementation on Content Studio tooling (`E1A-04`, `E1A-05`, `E1b`) or cross-platform native qualification (`M0-03`, `M0-04`). Implement full backlog items per commit without micro-slicing or writing redundant `docs/evidence/*.md` files.

`M1` (`M1-01` through `M1-07`) and `M2` (`M2-01` through `M2-04`) are complete on Mac, along with `E1A-01` through `E1A-03`. The shared content compiler (`pgp_content` / `pgp_runtime`) already validates and compiles all authored definitions directly from JSON; `M3` gameplay and content do not require `E1A-04` or `E1A-05` to be completed first. Keep `E1A-04`, `E1A-05`, and `E1b` queued after the `M3` playable slice works end-to-end.

## AI event assignments

AI-assisted development is responsible for a playable built-in event baseline;
human authors enrich it with distinctive stories. M1-07 implements modular
major-event detection, scheduled/seeded world occurrences and news dispatch.
M3-05 creates and validates the slice's eight base definitions, covering
technology developments, opportunities, disruptions, competition and recovery.
Use the [event-system contract](../docs/EVENT_SYSTEM.md) and its task handoff.
These are repository implementation/content tasks, with the same shared
registry, Studio validation and versioning as human work. The distributed game
generates its occurrences locally without an AI service or live news feed.

## Task and handoff format

Use [TASK_TEMPLATE.md](TASK_TEMPLATE.md). Before editing, inspect the actual repository and pending changes. An assignment names a backlog ID, outcome, constraints, owned files/interfaces, acceptance evidence and boundaries. At completion, record checks, source/content version, commit/push, unresolved limits and next task. Update `docs/STATUS.md` and the matching checkbox only after acceptance passes.

If a task cannot finish in one focused session, split by an observable behavior/interface and preserve the work in Git. Do not mark the entire milestone complete because one slice compiles. Do not invent test results or promise a platform that has not run natively.
