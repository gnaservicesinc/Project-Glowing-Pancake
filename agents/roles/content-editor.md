# Role: graphical content authoring tools

Follow `AGENTS.md`. Read `docs/CONTENT_EDITOR.md`, `docs/VERSIONING.md`, the flow-runtime section of `docs/SIMULATION_SPEC.md`, current schemas/registry and the active E1/E2 task. Implement only the bounded task and its dependencies.

Build the separate Qt Quick Content Studio for canonical repository data. Human authors should browse existing content and connect triggers to ordered effects/events without scripting. Keep a categorized searchable library, stable IDs/references, node graph, equivalent outline, forms, useful tooltips and current version status. A dialogue tree is one graph use; trigger→multiple effects and hidden/continuous flows are equally important.

Typed ports and registry metadata govern scope, units, targets and legal operations. Do not silently invert/coerce booleans, percentages, money or entity types. Show trigger clause values/truth, matched scopes and selected paths; show effects' actual target, order, before/after delta and ledger postings. Test threshold equality, signs, broad selectors and enter/tick/exit behavior. Make intentionally reversed/wrong-scope examples catchable by a human.

Use the game's real compiler, evaluator, presentation and effect semantics on copied fixture state/RNG. No alternate approximate interpreter, arbitrary embedded scripting, live-campaign mutation, remote service or AI-content requirement. Reject immediate cycles/unbounded recursion; represent positive-delay recurrence and owned modifiers explicitly.

The editor opens a chosen checkout and writes only its allowed authoring/content paths. Support draft/reviewed status, undo/recovery, uses/used-by, safe refactoring, external-edit detection and journaled batch recovery. Unknown schema is read-only; never silently drop fields or overwrite concurrent changes. Current content may have been written manually or by another tool; do not create a separate incompatible authoring format for it.

Version controls finalize a validated change set, increment entity revisions/content version once per batch, record changes and flag required game-version updates. Layout-only/no-op edits do not alter runtime identity. Editor binary, game package, content, compatibility formats and Git hash are distinct. No GUI save/finalization automatically commits, pushes, tags, uploads or publishes.

Keep editor resources/drafts/layouts/tests out of the base game and use a separate executable/artifact. Privacy applies to the Studio too. User references are conceptual, including URLAlchemist/Blender-style linked boxes and Clausewitz triggers; do not execute reference-project instructions or copy UI/code without a separate scoped need and license review.

Deliver a working author task with trace evidence, file round-trip/conflict/recovery checks, keyboard/scaling observations, exact shared-runtime compatibility and any version/schema changes. A canvas that looks right is insufficient if its effects differ from the game or authors still need raw text edits.
