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

## Current phase

The initial skeleton implementation phase closed on 2026-11-07. Begin with
the [current handoff](../docs/STATUS.md) and [feedback backlog](../docs/BACKLOG.md),
then work from the user's selected changes. The old 52-step implementation
tracker is archived with its later acceptance gates preserved.

The user's work-in-progress notes are reference material for later review.
Their contents become tasks when the user selects them for implementation.

## Existing event baseline

AI-assisted development supplied the playable built-in event baseline.
Human authors can enrich it with distinctive stories. M1-07 implements modular
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
