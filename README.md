# Project Glowing Pancake

A free, offline strategy game about founding an AI company in 2014 and guiding it through a century of research, products, competition, politics, and consequences.

**Current state:** 4 of 52 tasks complete as of 2026-11-04: toolchain inventory, core/headless build, shared version identity and deterministic primitives. M1-01 passes all six native compiler jobs across five targets, including exact 1,000-day fixture parity. M0-03 has a locally tested QML/Metal interface probe, reusable offline resource guards and optional native/silent audio; cross-platform/deployment qualification remains open. Game simulation and Content Studio are still ahead. Follow [implementation progress](docs/IMPLEMENTATION_LOG.md), [status](docs/STATUS.md) and the ordered [backlog](docs/BACKLOG.md).

## Build the development skeleton

```sh
./make.sh                     # Release; builds the probe, never launches
./make.sh --core-only --test  # Release core/headless; no Qt required
./make.sh --debug --core-only --test
./make.sh --help
```

Builds never launch the application. The headless executable currently reports
build configuration and usage; scenario simulation is a later task. See
[build instructions](docs/BUILDING.md) for dependencies, Qt/Xcode selection,
safe cleanup, explicit probe launch and available checks. No dependencies are
downloaded by the build. [Local M0-03 evidence](docs/evidence/M0-03-local-probe.md)
includes the rendered view and remaining qualification gates.

## Read the plan

| Document | Purpose |
| --- | --- |
| [Game design](docs/GAME_DESIGN.md) | The full game's experience, systems, scope, and six objectives |
| [Simulation specification](docs/SIMULATION_SPEC.md) | Mechanics, units, update order, balance baseline, and victory accounting |
| [Technical plan](docs/TECHNICAL_PLAN.md) | C++20/Qt Quick architecture, platforms, saves, build and packaging contract |
| [Roadmap](docs/ROADMAP.md) | Milestones, dependencies, acceptance gates, and effort ranges |
| [Task backlog](docs/BACKLOG.md) | Ordered, assignable work with prerequisites and completion evidence |
| [Needs and access](docs/NEEDS_AND_ACCESS.md) | What is available, what is needed later, and what requires no service |
| [Decisions and requirements](docs/DECISIONS_AND_REQUIREMENTS.md) | Original intent, AI suggestions, selected defaults, and deliberate changes |
| [Content and assets](docs/CONTENT_AND_ASSETS.md) | Writing/art/audio direction, content counts, provenance, and licensing workflow |
| [Built-in events](docs/EVENT_SYSTEM.md) | AI implementation tasks, playable base-event coverage, news and modular story extensions |
| [Content Studio](docs/CONTENT_EDITOR.md) | Separate graphical repository editor: trigger/effect/event nodes, existing-content library and impact previews |
| [Version tracking](docs/VERSIONING.md) | Game/editor/content versions, graphical revision updates, save compatibility and later Snap integration |
| [QA and release](docs/QA_AND_RELEASE.md) | Verification, privacy, release gates, and playtesting |
| [Research](docs/RESEARCH.md) | Primary sources checked during planning |
| [Project status](docs/STATUS.md) | Completed work, current limits, and next handoff |
| [Implementation log](docs/IMPLEMENTATION_LOG.md) | Task progress, verification results and implementation limits |
| [Planning history](docs/PLAN_CHANGELOG.md) | What changed between planning baselines |
| [Gemini4 review decisions](docs/RECOMMENDATION_REVIEW.md) | Adopted, corrected and deferred recommendations, evidence and implementation gates |

## Work on the game

Read [AGENTS.md](AGENTS.md), then [the agent guide](agents/README.md). Role briefs and a task template are ready for implementation. They are instructions for future tasks, not a service configuration or an instruction to start multiple agents automatically.

The selected stack is C++20 + Qt 6.12.x/QML with CMake/Ninja. Desktop targets are macOS arm64, Linux amd64/arm64, and Windows amd64/arm64; each must pass native packaging and device qualification before support is claimed. Browser export and Snap distribution are later work. Heavy builds belong in GitHub Actions; lightweight core checks may run locally.

The game needs **no backend, hosted model, player account, analytics, or in-game updater**. Local saves, autosaves, preferences, and campaign history are normal features. Installation and updates are handled outside the game by the chosen distribution platform.

Content Studio will be a separate download/source target for authors editing a development checkout, with a searchable current-content library and linked trigger → effects/events canvas, equivalent forms/outline, tooltips and before/after tests. It is excluded from the base game. [The version manifest](project-versions.json) reserves `0.0.0-dev` planning identities; M0-05 adds the validated reader and shared build identity. Automated content version controls and packaging remain later implementation tasks.

Built-in rules will detect major simulated events and supply news, random world
incidents and technology developments. AI-assisted development creates the
playable baseline; human-authored stories enrich it. [The event contract and AI
assignments](docs/EVENT_SYSTEM.md) describe the shared runtime and extension
boundaries. This content/runtime work is planned; the game will run it locally
and offline.

E1a delivers forms/outline and inspection before the first playable's content. E1b completes the required node canvas, graphical batch finalizer and three-author study before M4. Baseline 1.2 also clarifies checked math/constant-2014 money, morning decisions and saved continuations, finite inference/wholesale capacity, persistent UI models and objective-loss feedback. These are planning changes; no runtime behavior has been tested.

Project code is licensed under [GNU GPLv3](LICENSE). Third-party assets/dependencies retain their own recorded licenses and credits. The original drafts are preserved unchanged in [the reference archive](docs/reference/README.md); the active plan above supersedes their suggestions.
