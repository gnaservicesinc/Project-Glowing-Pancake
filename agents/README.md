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

> Complete M0-01 in docs/BACKLOG.md. Follow AGENTS.md and agents/roles/build-release.md. Inventory the current toolchain, Qt modules/licensing, hosted native-runner paths, disk constraints, and device-access gaps. Do not install large SDKs locally or publish anything. Record verified facts and the smallest next implementation task. Commit and push the task changes and report evidence.

After M0-01, assign M0-02. Do not jump directly from the design to implementing the century-wide simulation. The planning baseline is complete; code/build scaffolding has not yet been created.

M0-05 establishes the version-reader foundation; M1-06 establishes the shared flow runtime. E1a builds complete forms/outline and inspection before M3 finalizes its eight events. E1b completes the required node canvas, graphical finalizer and three-author study before M4; it need not block the first gameplay tests. E2 expands authoring as schemas mature and closes before M8's content pass. Follow those gates; do not hand authors a graph that bypasses validation or differs from gameplay.

## Task and handoff format

Use [TASK_TEMPLATE.md](TASK_TEMPLATE.md). Before editing, inspect the actual repository and pending changes. An assignment names a backlog ID, outcome, constraints, owned files/interfaces, acceptance evidence and boundaries. At completion, record checks, source/content version, commit/push, unresolved limits and next task. Update `docs/STATUS.md` and the matching checkbox only after acceptance passes.

If a task cannot finish in one focused session, split by an observable behavior/interface and preserve the work in Git. Do not mark the entire milestone complete because one slice compiles. Do not invent test results or promise a platform that has not run natively.
