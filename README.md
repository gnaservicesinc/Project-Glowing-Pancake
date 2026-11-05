# Project Glowing Pancake

A free, offline strategy game about founding an AI company in 2014 and guiding it through a century of research, products, competition, politics, and consequences.

**Current state:** 5 of 52 tasks accepted as of 2026-11-04, with additional implemented work under M1-03 through M1-07, M2-01 through M2-04 and E1A-01 through E1A-02. A headless three-year business runner now combines accounts, shared compute/staff, contracts, installed shares and warned liquidity recovery. Authored policies exercise expansion, equity rescue and closure through ordinary commands and exact saves. The foundation executes typed flows, weighted branches, durations, scoped variables, expressions, related-entity quantifiers, delays, choices and modifiers. Local settings, recovery copies, autosave policy and immutable worker/models exist. Platform prerequisites remain deferred. The local campaign shell connects Office, Lab and Inbox to the business simulation. Content Studio can browse validated local collections and edit typed flows through forms. The full player scenario, qualified research/models and author impact previews are still ahead. Follow [playability checkpoints](docs/PLAYABILITY.md), [implementation progress](docs/IMPLEMENTATION_LOG.md), [status](docs/STATUS.md) and the ordered [backlog](docs/BACKLOG.md).

## Build the development skeleton

```sh
./make.sh                     # Release; builds the campaign shell and probe
./make.sh --core-only        # Release core/headless; no Qt required
./make.sh --editor           # Separate Content Studio
ctest --test-dir build/core-release -R '^(core.liquidity|runtime.liquidity-integration|session.business-scenario)$' --output-on-failure
build/core-release/bin/pgp-headless --business-run tests/content/fixtures/business.json expansion
./make.sh --help
```

Builds never launch the application. The headless executable reports build
configuration, validates local content and runs command/event/save engineering demonstrations
and the three-year business scenario. The fixture starts with an engineering
product. The campaign shell can open it explicitly from disk. See
[build instructions](docs/BUILDING.md) for dependencies, Qt/Xcode selection,
safe cleanup, explicit desktop launch and available checks. No dependencies are
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

Content Studio builds as a separate target with a checkout library and a typed draft outline. Authors can configure triggers, lifecycles, execution nodes and scoped variables, then save and reopen drafts. The trigger → effects/events canvas and before/after tests are planned. The game build excludes the Studio target and its resources. [The version manifest](project-versions.json) reserves `0.0.0-dev` planning identities; M0-05 adds the validated reader and shared build identity. Automated content version controls and packaging remain later implementation tasks.

Built-in rules already dispatch committed project and contract facts, bounded
seeded world incidents and weekly news in engineering content. AI-assisted
development creates the playable baseline, with human-authored stories as
extensions. The eight-flow playable pack remains ahead. [Event contract and AI
assignments](docs/EVENT_SYSTEM.md)

E1a delivers forms/outline and inspection before the first playable's content. E1b completes the required node canvas, graphical batch finalizer and three-author study before M4. Baseline 1.2 also clarifies checked math/constant-2014 money, morning decisions and saved continuations, finite inference/wholesale capacity, persistent UI models and objective-loss feedback. These are planning changes; no runtime behavior has been tested.

Project code is licensed under [GNU GPLv3](LICENSE). Third-party assets/dependencies retain their own recorded licenses and credits. The original drafts are preserved unchanged in [the reference archive](docs/reference/README.md); the active plan above supersedes their suggestions.
