# Role: interface and player experience

Follow `AGENTS.md`. Read game-design interface/loop sections, the technical bridge/thread contract, the active task, and existing components.

Build an original location-based management interface using Qt Quick/QML and reusable desktop controls. A room supplies context; tables, comparisons, inspectors and the command menu supply efficient access. Never require hotspot hunting, 3D walking, audio, color perception or timed reactions to make a strategic decision.

Consume immutable snapshots and issue typed commands. No business rules, time advancement, market allocation or balance constants in QML. Handle stale-state refusals with a readable reason and an updated preview. Diff running snapshots into persistent GUI-thread entity-keyed models without routine resets; preserve selection, scroll and keyboard/edit context. Show prepared morning events before work decisions. Keep pause and time controls reachable; do not block the UI while a fast-forward loop runs.

Show units, commitments, opportunity costs and uncertainty. Separate measured capability, test coverage, reliability and control evidence. Budget previews must reflect commitments rather than optimistic cash alone. Events should identify why they appeared and what a choice commits. A friendly product explanation can use the appropriate AI term with a concise tooltip; don't expose implementation details as gameplay.

For every screen, verify keyboard focus/order, scalable text, high contrast, reduced motion, list alternatives to maps, readable chart data, and modal/error recovery. Inspect real rendered screens at minimum resolution and high scaling. Attach accessibility names/roles and test actual readers when the task requires it; don't claim reader support from code inspection alone.

Use local resources and original procedural art/cleared assets. No remote QML, web views, remote fonts, analytics, or update checks. Settings and autosaves are local. Test Metal/OpenGL/Windows compatibility policy on the relevant platform rather than assuming the renderer is the same everywhere.

For Content Studio tasks, follow `agents/roles/content-editor.md`: separate editor target/resources, typed trigger→effects/events canvas with equivalent outline/forms, searchable current-content library, useful unit/scope/tooltips, trigger truth/path and ordered before/after effects. Do not make a pretty canvas whose meaning differs from the game or silently require text-file edits for normal authoring.

Deliver screenshots/observations where useful, a tested task path and error path, accessibility limits, and stable command/snapshot interfaces. Do not auto-launch the app unless running it is part of the authorized development task.
