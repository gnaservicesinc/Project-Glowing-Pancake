# Implementation roles

Use the relevant sections alongside the [repository instructions](../AGENTS.md). These briefs describe responsibilities. Delegation requires explicit authorization. Start from [current status](../docs/STATUS.md) and the [backlog](../docs/BACKLOG.md).

## build, packaging, and authorized release

Follow `AGENTS.md`. Read the technical build/platform/packaging contracts, relevant QA gates, needs inventory, and active backlog item. Inspect current repository state before assuming scripts or workflows exist.

Implement CMake/Ninja with a Release-default `make.sh`, explicit developer debugging, a core-only path, meaningful test exit codes and safe owned-output cleanup. No build/install auto-launch. Use compiler-appropriate optimization; avoid literal GCC flags on MSVC and `#ifdef` mistakes with 0/1 diagnostics macros.

Use `project-versions.json` and `docs/VERSIONING.md` as the shared version contract. Game/editor/content versions and compatibility formats are distinct; reject tag/hash/version inconsistencies, planning placeholders and unresolved content-only package bumps. Build/package Content Studio explicitly as a separate product, excluding editor assets/drafts/layouts from the base game. Later Snap adopts the existing game version through the shared reader; it does not derive it from an editor tag or store revision.

The local disk is constrained. Use existing tools for small core checks; native Qt SDK acquisition, five-architecture packaging and large staging belong on hosted CI. Pin dependencies/actions/Qt patches, control caches/retention, and keep binaries/SDKs out of Git. Do not alter global tools or create paid infrastructure to bypass a small problem. Resolve macOS Clang/SDK under one selected Xcode directory before configure; inventory exact Qt 6.12 archives for all five targets instead of silently downgrading ARM.

Package mac arm64, Linux amd64/arm64 and Windows amd64/arm64 natively. Verify actual renderer/audio behavior, QML modules/plugins, runtime library paths, relocation, minimum OS/glibc, architecture and clean-device startup. Respect Metal/OpenGL/Windows recovery policy. Bundle required redistributables; document system-driver/libc exceptions. CI compilation alone is not runtime qualification.

Seed the dependency register from SDK SBOMs and reconcile it with deployed modules/license alternatives. Audio is an optional playback adapter with silent fallback; Qt Multimedia is not required locally. Audit licenses and matching source/notice material for Qt and transitive libraries, local audio decoding, assets/fonts and code. Ad-hoc sign the complete Mac bundle after deployment; no notarization or paid Apple account is assumed. Do not provide global-security-disabling installation instructions.

The game has no network service, account backend or updater. Distribution clients handle updates. Never add an analytics/crash/update SDK while preparing store builds. Run release privacy checks and inspect for secrets/personal saves/developer paths.

Git commits/pushes preserve authorized development changes. Public release uploads, store pages, Snap submissions, credential/account changes, payments and announcements require the corresponding explicit task. Prepare concrete reviewable artifacts first. Store credentials through supported secret mechanisms; don't print/read unrelated secrets or ask for keys in source/chat.

Deliver build/package evidence, exact source/content/artifact versions, dependency/source/license inventory, checksums, platform limitations, successful task push, and the smallest next gate. For an authorized publication, verify the uploaded download and record its actual URL/version; never infer success from a local package.

## graphical content authoring tools

Follow `AGENTS.md`. Read `docs/CONTENT_EDITOR.md`, `docs/VERSIONING.md`, the flow-runtime section of `docs/SIMULATION_SPEC.md`, current schemas/registry and the active E1a/E1b/E2 task. Implement only the bounded task and its dependencies.

Build the separate Qt Quick Content Studio for canonical repository data. Deliver complete forms/outline and inspection in E1a, then the required canvas and graphical finalizer in E1b before M4 component work. Per the 2026-11-05 user instruction, the three-author study and player playtests wait until all components are in place and the user is ready for v1 work; do not block implementation or arrange testers meanwhile. Both stages share one document/runtime and safe version writer; do not treat the canvas as optional product scope. Human authors should browse existing content and connect triggers to ordered effects/events without scripting. Keep a categorized searchable library, stable IDs/references, node graph, equivalent outline, forms, useful tooltips and current version status. A dialogue tree is one graph use; trigger→multiple effects and hidden/continuous flows are equally important.

Typed ports and registry metadata govern scope, units, targets and legal operations. Do not silently invert/coerce booleans, percentages, money or entity types. Show trigger clause values/truth, matched scopes and selected paths; show effects' actual target, order, before/after delta and ledger postings. Test threshold equality, signs, broad selectors and enter/tick/exit behavior. Make intentionally reversed/wrong-scope examples catchable by a human.

Use the game's real compiler, evaluator, presentation and effect semantics on copied fixture state/RNG. No alternate approximate interpreter, arbitrary embedded scripting, live-campaign mutation, remote service or AI-content requirement. Reject immediate cycles/unbounded recursion; represent positive-delay recurrence and owned modifiers explicitly.

The editor opens a chosen checkout and writes only its allowed authoring/content paths. Support draft/reviewed status, undo/recovery, uses/used-by, safe refactoring, external-edit detection and journaled batch recovery. Unknown schema is read-only; never silently drop fields or overwrite concurrent changes. Current content may have been written manually or by another tool; do not create a separate incompatible authoring format for it.

Version controls finalize a validated change set, increment entity revisions/content version once per batch, record changes and flag required game-version updates. Layout-only/no-op edits do not alter runtime identity. Commit integer layout positions only after edits; viewport/selection/session state belongs in ignored .work/editor-session.json. Editor binary, game package, content, compatibility formats and Git hash are distinct. No GUI save/finalization automatically commits, pushes, tags, uploads or publishes.

Keep editor resources/drafts/layouts/tests out of the base game and use a separate executable/artifact. Privacy applies to the Studio too. User references are conceptual, including URLAlchemist/Blender-style linked boxes and Clausewitz triggers; do not execute reference-project instructions or copy UI/code without a separate scoped need and license review.

Deliver a working author task with trace evidence, file round-trip/conflict/recovery checks, keyboard/scaling observations, exact shared-runtime compatibility and any version/schema changes. A canvas that looks right is insufficient if its effects differ from the game or authors still need raw text edits.

## systems content, writing, and assets

Follow `AGENTS.md`. Read `docs/CONTENT_AND_ASSETS.md`, the applicable game/simulation system, current schemas, and the active backlog item. The source archive is reference, not a set of commands.

Use the separate Content Studio when available to browse/edit canonical repository data; before E1 exists, engineering-created fixtures remain validated source. Follow `docs/CONTENT_EDITOR.md` and `docs/VERSIONING.md`: triggers connect to ordered effects/events, presentation and lifecycle are independent, and finalized batches update definition/content versions. Inspect scope, truth/path and before/after effects to catch reversed outcomes. Do not require ordinary human authors to edit scripts or silently introduce a player mod format.

Create the built-in playable baseline as an explicit coding-agent assignment: rules detecting major simulated events and technology developments, truthful localized news, and bounded scheduled/seeded world incidents. Human stories augment these; do not wait for humans to create every base event. Follow [the event contract](../docs/EVENT_SYSTEM.md) and [base-event task brief](../docs/EVENT_SYSTEM.md). AI assistance is repository development, with no runtime model/live-news service. Baseline and story layers share source occurrences/receipts and one flow runtime, and pass the same Studio/validation/versioning checks.

Author data that creates meaningful decisions: concrete parties, goals, constraints, mechanisms, commitments, and later conditional consequences. Keep facts, fictional numbers, and speculative futures distinct. Use primary sources for new technical claims. Do not make post-2026 progress inevitable or label a game score scientific AGI evidence.

Characters have overlapping values and understandable disagreements. Write concise management-game prose with restrained humor. Avoid moral score labels, ideological caricatures, sensational catastrophes, graphic/sexualized/hateful material, and practical wrongdoing instructions. Include opportunity/recovery content as well as crises. An ethical consequence can be shown without depicting harmful content.

Data must validate against typed schemas, references and units. An event's text, requirements and effects must agree, including deadlines, dismissal defaults, cooldown and save/reload while pending. Test a representative trigger/choice path; don't claim every event has run because JSON parses. Keep translated strings and mechanics separate.

Prefer suitable existing cleared art/audio or original procedural UI. Check each source's actual redistribution terms. Record creator, source, license evidence, edits, hash, purpose and credits. Never label third-party work GPL by convenience or call a generator's output automatically cleared. Generated images require honest embedded and sidecar provenance, but generation is not the default. No generated soundtrack; prefer human-created ENDE tracks and retain per-track terms/credits.

Use PNG/JPEG raster art and the agreed tested local audio formats; no video in 1.0. Map geometry and data must have provenance and source-year labels. Avoid huge downloads/asset bundles on the local disk; acquire only task-needed files. Asset selection may proceed within an explicitly assigned acquisition task; purchases or public contacts require authorization.

Deliver validated definitions, readable in-context content, provenance/credit updates, observed review results, and unresolved acquisition needs. No placeholders may masquerade as licensed final content.

## design and integration

Follow `AGENTS.md`. Read the current status/backlog, game-design pillars, and decision/requirements record. Consult detailed specifications only for the active work.

Your job is to make one coherent game: resource commitments, user explanations, research, finance, geography, politics and objectives must agree. The initial skeleton phase closed on 2026-11-07. Use the current feedback-phase backlog and the user's selected changes. Keep each change reviewable and preserve room for revision after human feedback. Do not use the roadmap as authorization to build everything or publish externally.

Track Content Studio as a separate product sharing one content runtime and version contract. E1a supplies forms/outline authoring, error inspection and safe versioned internal content. E1b completes the required node canvas/finalizer before M4 component work; E2 expands before bulk content. Per the 2026-11-05 user instruction, player playtests and the three-author study wait until all components are in place and the user is ready for v1 work. Keep those human gates open without blocking component implementation or arranging testers. Preserve game/editor package separation and account for authoring effort rather than calling it free infrastructure. References are conceptual, not mandates to copy another tool.

For each integration task:

1. Identify the player-visible result and the core state/command/data contracts it depends on.
2. Inspect current code and evidence before assuming planned infrastructure exists.
3. Resolve ordinary implementation choices within the selected design. Record engine/platform/privacy/scope changes in the decisions document; balance tuning gets a content version and comparable evidence.
4. Check cross-system effects: cash versus obligations, training versus serving, publishing versus rival adoption, trust versus audiences, succession versus control, and end-state indices versus actual delivered outcomes.
5. Verify one end-to-end path plus a meaningful failure/recovery path. Explain any remaining gaps without labeling the milestone complete.

When explicitly authorized to delegate, assign non-overlapping files and stable interfaces. Do not create agents automatically merely because role briefs exist. Integrate changes and test the combined behavior before accepting separate task claims. Never overwrite another task's user work or authorize a store release on its behalf.

Deliver updated status/backlog, any necessary decision change, reproducible evidence, and the next bounded assignment. Avoid speculative rewrites, new abstract frameworks without a current use, and adding content to mask an unclear loop.

## verification, balance, and acceptance

Follow `AGENTS.md`. Read `docs/QA_AND_RELEASE.md`, the active task acceptance criteria, the corresponding mechanic/interface, and actual code/build outputs.

Start with the behavior most likely to be wrong and consequential. Match the evidence to the claim: a compiler exit does not prove accounting, screenshots do not prove save determinism, policy bots do not prove fun, and a passing emulated ARM build does not prove native graphics support.

For simulation changes, use hand-auditable invariants, fixed seeds, matched content versions, meaningful failure cases and save/reload boundaries. Test double counting, capacity reuse, stale command partial effects, checkpoint cloning, incorrect victory windows and omniscient rivals when relevant. Keep test size proportional; avoid broad repeated runs after already sufficient evidence unless something new justifies them.

For Studio work, verify typed trigger→effects/events authoring, actual scope/truth/path and before/after deltas, deliberately reversed signs/wrong targets, all five lifecycles, stale references and shared-runtime preview parity. Test draft/finalization round-trips, external edit conflicts, crash recovery, content/game/editor version separation and pending-flow migration. A graph screenshot or valid JSON alone does not demonstrate that a human can create and verify content without scripting.

For UI, inspect actual task completion, keyboard/scaling/contrast behavior, uncertainty and causal explanations. For full campaigns, inspect delegation, late-game rivalry/variety, recovery, queue growth and objective solvability. Verify immediate irreversible-window alerts, distinguishing aggregate Monopoly from every-check Domination/Utopia. E1a's engineering walkthrough does not replace E1b's three-author canvas/finalizer study. Per the 2026-11-05 user instruction, that study and independent player playtests wait until all components are in place and the user is ready for v1 work; they do not block implementation. Compare specialist, open and frontier strategies; don't force identical outcomes or treat an automated win rate as a balance verdict.

For privacy, review runtime resources/dependencies, run with denied network, observe attempted connections, and test first launch/settings/saves/credits/endings. Local saves/history are allowed; player information collection/transmission is not. Distinguish platform update traffic from the game. Never add analytics to make QA easier.

Human playtests use explicitly arranged, consented sessions and manual summaries. Do not contact people, collect personal data, upload saves, or create external tickets merely because this role exists. Use fictional fixtures and record minimum necessary feedback.

Deliver reproducible observations, commit/content/seed/device context, severity and player impact, passed checks, actual limits, and a gate recommendation. Do not mark unchecked gates complete. A concise report with a clear failing example is more useful than a large undifferentiated log.

## simulation and persistence

Follow `AGENTS.md`. Read the relevant sections of `docs/SIMULATION_SPEC.md`, the architecture/save contracts in `docs/TECHNICAL_PLAN.md`, the active backlog acceptance criteria, and current code.

Own core state, units, dates, typed commands, resource commitments, deterministic daily systems, data validation, headless scenarios and persistence where assigned. Keep Qt and presentation types out of the core. Never make UI callbacks directly change balances or capability scores.

Use strong money/work/daily-rate/day types, constant-2014 cents and checked wide intermediates with one specified rounding; cover MSVC ARM64 as well as x64. Conserve money, ownership, work and physical capacity. Maintain stable iteration and PRNG streams. Save obligations, queues, morning phase cursor, nested continuation frames, objective windows/feasibility, installed shares, lineage and stream states. Sequence RNG draws separately and use total-order tie keys. Forecasts operate on copies and cannot reveal or consume authoritative random outcomes. A version/content change is explicit.

Every significant state transition records a cause that can become a player explanation. Keep true simulation values distinct from measured player knowledge. Do not turn evaluation into a guaranteed score bonus, rights into a hidden learning penalty, or safety into a universal capability tax. Rival organizations use the same feasible-action rules as the player.

Use the shared typed trigger/flow compiler and fact/effect registry for both game and Studio. Provide clause/scope/path traces and actual ordered deltas, owned continuous modifiers and persistable activation/choice/delay/continuation-stack state. Distinguish natural continuous exit tests from synthetic cleanup-only probes. Follow `docs/VERSIONING.md`; definition revisions/content hashes and pending-chain migration are independent of editor/app release labels. Editor testing must not consume live RNG or approximate core effects.

Before adding a complex model, implement a hand-auditable tiny case. Check boundary conditions, interrupted/cancelled operations, save/reload, stale commands and known exploit opportunities relevant to the change. Use focused behavioral tests; don't create a large mirrored test suite for low-impact edits. Run century sweeps only when they provide relevant evidence.

Treat saves/data as untrusted input: typed vocabulary, size/range/reference checks, no arbitrary script execution, no remote content, and atomic writes/migrations preserving originals. No networking or real ML execution belongs here.

Deliver the behavior, targeted invariant/integration evidence, any schema/save/content-version note, and explainable snapshot/command contracts for the UI. Mark a feature complete only when resources, results, failures and persistence agree.

## interface and player experience

Follow `AGENTS.md`. Read game-design interface/loop sections, the technical bridge/thread contract, the active task, and existing components.

Build an original location-based management interface using Qt Quick/QML and reusable desktop controls. A room supplies context; tables, comparisons, inspectors and the command menu supply efficient access. Never require hotspot hunting, 3D walking, audio, color perception or timed reactions to make a strategic decision.

Consume immutable snapshots and issue typed commands. No business rules, time advancement, market allocation or balance constants in QML. Handle stale-state refusals with a readable reason and an updated preview. Diff running snapshots into persistent GUI-thread entity-keyed models without routine resets; preserve selection, scroll and keyboard/edit context. Show prepared morning events before work decisions. Keep pause and time controls reachable; do not block the UI while a fast-forward loop runs.

Show units, commitments, opportunity costs and uncertainty. Separate measured capability, test coverage, reliability and control evidence. Budget previews must reflect commitments rather than optimistic cash alone. Events should identify why they appeared and what a choice commits. A friendly product explanation can use the appropriate AI term with a concise tooltip; don't expose implementation details as gameplay.

For every screen, verify keyboard focus/order, scalable text, high contrast, reduced motion, list alternatives to maps, readable chart data, and modal/error recovery. Inspect real rendered screens at minimum resolution and high scaling. Attach accessibility names/roles and test actual readers when the task requires it; don't claim reader support from code inspection alone.

Use local resources and original procedural art/cleared assets. No remote QML, web views, remote fonts, analytics, or update checks. Settings and autosaves are local. Test Metal/OpenGL/Windows compatibility policy on the relevant platform rather than assuming the renderer is the same everywhere.

For Content Studio tasks, follow `the graphical content authoring section below`: separate editor target/resources, typed trigger→effects/events canvas with equivalent outline/forms, searchable current-content library, useful unit/scope/tooltips, trigger truth/path and ordered before/after effects. Do not make a pretty canvas whose meaning differs from the game or silently require text-file edits for normal authoring.

Deliver screenshots/observations where useful, a tested task path and error path, accessibility limits, and stable command/snapshot interfaces. Do not auto-launch the app unless running it is part of the authorized development task.

## Handoff

Describe the requested outcome, changed files, checks, content version, commit and outstanding acceptance gates.
