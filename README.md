# Project Glowing Pancake

A free, offline strategy game about founding an AI company and guiding it through a century of research, products, competition, politics, and consequences.

The selected campaign redesign starts with a California birth in 1997, company
play at 18 in 2015 and an ending at 118 in 2115, followed by optional sandbox
continuation. [Campaign flow](docs/CAMPAIGN_FLOW.md),
[technology design](docs/TECHNOLOGY.md) and the
[140-candidate technology catalog](docs/TECHNOLOGY_CATALOG.md) describe the new
direction. New Game now implements the founder introduction and company setup;
the company world, automatic resume and generated HQ people are available for review.

**Current state.** The initial skeleton phase closed on 2026-11-07. The current
core is ready for human feedback and the user's selected refinements. The
[current handoff](docs/STATUS.md) and [feedback backlog](docs/BACKLOG.md)
replace the initial implementation queue.

The Mac development build opens with the founder prologue and the 2015 company
opening. It also includes engineering scenarios, local persistence, Lab projects and deployments, business
and staffing, rivals, infrastructure, civic and regional systems, succession
and objectives. The separate Content Studio shares the typed event runtime
and versioned content tools. The [playability guide](docs/PLAYABILITY.md)
describes the controls and starting paths.

The finalized internal content version is **0.32.0-dev.1**, generation **32**.
Game and editor versions remain development placeholders. Human validation,
extended QA, native packaging and release gates are carried into later
phases in the [backlog](docs/BACKLOG.md).

## Build and play

```sh
./make.sh                    # Both apps, Release, local CPU tuning; no tests or launch
./make.sh --package --jobs 4   # Release Mac .app with Qt, icon and resources; no launch
open "dist/Project Glowing Pancake.app"
./make.sh --core-only        # Release core/headless; no Qt required
./make.sh --editor           # Separate Content Studio
build/core-release/bin/pgp-headless --business-run tests/content/fixtures/business.json expansion
build/core-release/bin/pgp-headless --lab-demo data/runtime/first-playable.json
build/core-release/bin/pgp-headless --events-demo data/runtime/first-playable.json
build/core-release/bin/pgp-headless --specialist-demo data/runtime/first-playable.json
build/core-release/bin/pgp-headless --competition-demo data/runtime/first-playable.json standard 1096
./make.sh --help
```

Builds never launch the application. On Mac, open **Project Glowing Pancake.app**
in `dist` from Finder. A current company resumes automatically, paused at the
same place. Otherwise New Game opens the 1997 prologue and leads through founding
to the 2015 workspace. The HUD, room controls and device dock open the company's
tools. Select a person's vector portrait to inspect their profile. Escape pauses
and opens the menu; Quit uses an exit confirmation. [World and portrait design](docs/GAMEPLAY_INTERFACE.md).
The Codex Run action uses `./script/build_and_run.sh` to explicitly build,
package and launch. Use the in-game guide
or [player path](docs/PLAYABILITY.md) for the first loop. The headless executable
also runs isolated engineering and complete baseline demonstrations. See
[build instructions](docs/BUILDING.md) for dependencies, Qt/Xcode selection,
local save directories, explicit launch options and focused checks. No
dependencies are downloaded by the build.

## Read the plan

| Document | Purpose |
| --- | --- |
| [Game design](docs/GAME_DESIGN.md) | The full game's experience, systems, scope, and six objectives |
| [Campaign flow](docs/CAMPAIGN_FLOW.md) | Founder introduction, founding terms, stages, age-118 ending and sandbox continuation |
| [Technology design](docs/TECHNOLOGY.md) | Review of the supplied proposals, research variants, IP, progression and integration |
| [Technology candidates](docs/TECHNOLOGY_CATALOG.md) | 140 reviewed candidates across ten domains, plus bounded refinement families |
| [Simulation specification](docs/SIMULATION_SPEC.md) | Mechanics, units, update order, balance baseline, and victory accounting |
| [Technical plan](docs/TECHNICAL_PLAN.md) | C++20/Qt Quick architecture, platforms, saves, build and packaging contract |
| [Roadmap](docs/ROADMAP.md) | Closed skeleton phase and the feedback, refinement, events, balance and release sequence |
| [Task backlog](docs/BACKLOG.md) | Feedback-phase entry and carried acceptance gates |
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

Content Studio builds as a separate target with a checkout library and shared typed canvas/forms/outline. Authors configure triggers, lifecycles, execution nodes and scoped variables, save drafts, inspect real effects and finalize reviewed internal batches through graphical controls. Flow inspection shows trigger truth, actual effects, ledger entries and suspended continuations on isolated fixtures. The game application excludes Studio resources; the default build compiles both
applications as separate targets. [The version manifest](project-versions.json) tracks game, editor and finalized content identities separately. Separate Studio packaging and usability acceptance remain later work.

Built-in rules dispatch actual research, evaluation and pilot outcomes, seeded demand incidents, paid supplier/recovery opportunities and competitive notices through one shared runtime. The eight-flow pack is in `data/runtime/first-playable.json`, with localizable English messages and a complete baseline-only headless run. [Event contract](docs/EVENT_SYSTEM.md) · [Implementation and seeds](src/runtime/BASE_EVENTS.md)

Content Studio includes shared forms and inspection, a node canvas and graphical snapshot finalization. Human author validation, century endurance and native qualification carry into the later phases. [Rival planning and shared-market contract](src/runtime/COMPETITION.md) · [Heavyweight responses and agreements](src/runtime/HEAVYWEIGHTS.md) · [Corporate governance and acquisitions](src/runtime/CORPORATE.md).

Project code is licensed under [GNU GPLv3](LICENSE). Third-party assets/dependencies retain their own recorded licenses and credits. The original drafts are preserved unchanged in [the reference archive](docs/reference/README.md); the active plan above supersedes their suggestions.
