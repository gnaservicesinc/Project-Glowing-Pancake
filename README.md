# Project Glowing Pancake

A free, offline strategy game about founding an AI company in 2014 and guiding it through a century of research, products, competition, politics, and consequences.

**Current state:** 36 of 52 steps complete for Mac feature development as of 2026-11-05. The bundled 2014–2017 campaign is playable through the Office, Lab, Operations, Civic and Inbox: choose headquarters, license data, research, train, evaluate, qualify a service, compare contracts and respond to events/rivals. The guide can be hidden and reopened. Full keyboard runs at 100% and 200% text reach the ending solvent and reload exactly.

M3-03 adds bounded evaluation/lease-renewal policies, configurable pauses, real deadline/milestone stops, a recoverable supplier setback, paid hiring, fixed financing and transparent slice scores. The efficient document-specialist route is profitable through the ending without new financing. **Human playtests and author studies are deferred until all components are in place and the user is ready for v1 work.** They do not block implementation. The century campaign and release qualification remain later steps. The separate Content Studio now has a [typed node canvas](src/studio/CANVAS.md), shared forms/outline, runtime inspection and [graphical internal snapshot review/finalization](src/studio/SNAPSHOTS.md).

Platform and release checks remain separately deferred. The current finalized internal content snapshot is 0.8.0-dev.1; game/editor versions remain planning placeholders with an unresolved game-version requirement. The [engineering authoring walkthrough](src/studio/WALKTHROUGH.md) passes through real QML forms, draft saving/reopening and isolated snapshot validation; its extensions verify pointer ports, layout saves, exact waits, graphical finalization and interrupted-write recovery. M4-01 adds the default 15-company roster, staged entry, budgeted weekly strategies, one shared customer pool, editable service prices and delayed public research adoption. Reduced, expanded and disclosed finite catch-up presets are selectable at campaign creation. M4-02 adds visible heavyweight pressure/warnings, finite paid mobilization, withdrawal and funded publication/pilot agreements. M4-03 adds negotiated financing, executives, conserved vesting, board votes/covenants and acquisitions that retain subsidiary obligations. Paid group funding and earned dividends conserve cash and group profit. M5-01 adds [owned compute and an offline domestic map](src/runtime/INFRASTRUCTURE.md): compare all 50 states and DC, reserve grid power, buy and staff hardware, book inspections, and meter actual work. M5-02 adds [regional product channels and wholesale compute](src/runtime/REGIONAL_MARKETS.md). M5-03 adds [site hazards, paid protection, geographic replication and insurance](src/core/RESILIENCE.md). E2-01 adds [scenario/world/policy authoring](src/studio/WORLD.md), staged changes, a state map/list and actual opening previews. E2-02 adds searchable catalogs, reviewed refactoring, localized text, asset credits and typed reusable flows. [Catalog and reuse workflow](src/studio/CATALOGS.md). E2-03 adds finalized revision comparisons, matched fixtures and reviewed save copies. [Revision review](src/studio/SNAPSHOTS.md#compare-finalized-revisions-and-review-a-campaign-copy). M6-01 adds US elections, policy hearings, paid disclosures and dated launch and permit rules. [Civic room](src/core/CIVIC.md). Macro response, international coordination and media are next. Follow [playability checkpoints](docs/PLAYABILITY.md), [status](docs/STATUS.md), [implementation checks](docs/IMPLEMENTATION_LOG.md) and the ordered [backlog](docs/BACKLOG.md).

## Build and play

```sh
./make.sh                     # Release; builds without launching
build/release/bin/pgp-game     # Explicitly open the bundled campaign
./make.sh --core-only        # Release core/headless; no Qt required
./make.sh --editor           # Separate Content Studio
ctest --test-dir build/core-release -R '^(core.liquidity|runtime.liquidity-integration|session.business-scenario)$' --output-on-failure
build/core-release/bin/pgp-headless --business-run tests/content/fixtures/business.json expansion
build/core-release/bin/pgp-headless --lab-demo data/runtime/first-playable.json
build/core-release/bin/pgp-headless --events-demo data/runtime/first-playable.json
build/core-release/bin/pgp-headless --specialist-demo data/runtime/first-playable.json
build/core-release/bin/pgp-headless --competition-demo data/runtime/first-playable.json standard 1096
./make.sh --help
```

Builds never launch the application. `pgp-game` opens a headquarters choice
without requiring content files or developer commands. Use the in-game guide
or [player path](docs/PLAYABILITY.md) for the first loop. The headless executable
also runs isolated engineering and complete baseline demonstrations. See
[build instructions](docs/BUILDING.md) for dependencies, Qt/Xcode selection,
local save directories, explicit launch options and focused checks. No
dependencies are downloaded by the build.

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

Content Studio builds as a separate target with a checkout library and shared typed canvas/forms/outline. Authors configure triggers, lifecycles, execution nodes and scoped variables, save drafts, inspect real effects and finalize reviewed internal batches through graphical controls. Flow inspection shows trigger truth, actual effects, ledger entries and suspended continuations on isolated fixtures. The game build excludes the Studio target and its resources. [The version manifest](project-versions.json) retains `0.0.0-dev` game/editor planning identities and the finalized `0.8.0-dev.1` content identity. Packaging remains later work.

Built-in rules dispatch actual research, evaluation and pilot outcomes, seeded demand incidents, paid supplier/recovery opportunities and competitive notices through one shared runtime. The eight-flow pack is in `data/runtime/first-playable.json`, with localizable English messages and a complete baseline-only headless run. [Event contract](docs/EVENT_SYSTEM.md) · [Implementation and seeds](src/runtime/BASE_EVENTS.md)

E1a forms/inspection and the internal snapshot writer share the runtime. E1b completes the required node canvas and graphical finalizer before M4 component work; its three-author study waits for v1 preparation. Human validation, century systems and the six final objectives retain their later acceptance gates. [Rival planning and shared-market contract](src/runtime/COMPETITION.md) · [Heavyweight responses and agreements](src/runtime/HEAVYWEIGHTS.md) · [Corporate governance and acquisitions](src/runtime/CORPORATE.md).

Project code is licensed under [GNU GPLv3](LICENSE). Third-party assets/dependencies retain their own recorded licenses and credits. The original drafts are preserved unchanged in [the reference archive](docs/reference/README.md); the active plan above supersedes their suggestions.
