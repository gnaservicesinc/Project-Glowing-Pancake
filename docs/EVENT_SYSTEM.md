# Built-in events, news, and story layers

Updated 2026-11-05 · The eight-flow Mac baseline, bounded rivals, paid opportunities and exact saves run in the three-year headless scenario. Player presentation is M3-02; content snapshot finalization remains E1A-04/05.

## Playable baseline

The game supplies a useful world without requiring a human to write each occurrence. Built-in rules detect significant simulated developments, produce concise news, and create bounded scheduled or seeded world incidents. Human-authored characters, negotiations and stories enrich that baseline. Coding agents are assigned to implement the rules and create the initial repository content; this is development work, not an AI service used by the running game.

All occurrences use the shared [flow runtime](SIMULATION_SPEC.md#2a-content-flow-runtime-and-graphical-authoring-contract), typed fact/effect registry, transaction rules and save state. There is one event interpreter. The game works offline with local validated definitions and localized templates. It does not read live news, call a model, or generate unconstrained prose during play.

## Sources and responsibilities

| Source | What starts it | What it may do |
| --- | --- | --- |
| Committed simulation development | Research completion, evaluation finding, signed contract, liquidity warning, rival action | Publish a fact with its actual cause/result; detect a milestone or threshold crossing; project a truthful bulletin or eligible follow-up |
| Scheduled world development | Scenario date/window and explicit prerequisites | Activate an eligible local definition; skip, expire or wait according to its declared lifecycle |
| Seeded world incident | Eligible scope and a configured sampling boundary | Select a bounded incident or no incident using saved randomness; apply only registered effects through a validated flow |
| Authored story layer | Existing facts/occurrences and additional story conditions | Add characters, choices, negotiations or delayed consequences while preserving the baseline cause and receipt |

The originating simulation system owns its result. A research-completed bulletin must not grant the research a second time. An external incident that changes demand must commit a real demand effect before news reports the change; a headline is never the authority for a hidden mechanical change. A story can replace the presentation of an occurrence or attach a follow-up explicitly. It cannot create a second copy of the underlying incident, cost, reward or modifier.

Use fictional organizations and publications. Distinguish a sourced historical reference, a simulated outcome and a speculative future. Historical material must be researched when introduced; the representative coverage below makes no claim about real news. Technology beyond the starting catalog depends on the scenario and actual research/prerequisites, with no guaranteed future breakthrough date.

## First-playable coverage

The eight-definition M3 budget includes built-in coverage plus selected richer stories. It does not require eight elaborate human-written dialogues in addition to the baseline. These eight flows are implemented in `data/runtime/first-playable.json`. Concrete thresholds, weights, deadlines and terms are validated data. The pack remains internal development content; M3-02 supplies its complete player-facing presentation.

| Flow | Detection or occurrence | Player-facing result | Important fixture |
| --- | --- | --- | --- |
| `slice.research_milestone` | A research node completes for this company | Technology-development bulletin with newly available options and their prerequisites | Completion emits once; loading or repeating the briefing grants nothing again |
| `slice.evaluation_finding` | A held-out evaluation commits a concrete finding | Actionable summary of task/subgroup evidence, with an eligible existing data-offer/retest recovery option and its license/compute costs | Evidence agrees with the result; unavailable data is not offered; opening the report acquires no license or capability |
| `slice.pilot_result` | Pilot acceptance, delivery or service failure commits | Customer news with the actual contract outcome and obligations | Acceptance and failure use real receipts; no second payment or premature serving |
| `slice.liquidity_warning` | Runway/reserve crosses a warning boundary | Finance alert linking forecast, commitments and the workout path | Entry/recovery/re-entry boundaries; no daily repeat while unchanged |
| `slice.rival_notice` | A financed rival experiment completes or a heavyweight observes actual customer delivery | Competitive news with the disclosed 90-day warning | Private training does not trigger the heavyweight; warning never creates unbudgeted capacity |
| `slice.supplier_offer` | A seeded, eligible supplier opportunity at a defined review boundary | Optional cheaper/smaller capacity offer with actual terms | Decline has no commitment; acceptance validates capacity, cash and contract rights |
| `slice.market_shift` | A bounded seeded external demand condition for the slice's enterprise segment | World/business bulletin showing a temporary demand change and expiry | Owned modifier applies/expires once, stays within validated bounds and never changes already settled work |
| `slice.recovery_opportunity` | A recoverable setback and eligible smaller pilot or staged-payment offer | A practical recovery choice with costs, constraints and later consequences | A viable recovery route and a rejected/unaffordable path; no money or contract created by merely opening the offer |

These use the slice's research, evaluation, contract, finance and demand systems. M3-05 also supplies the minimal bounded rival routines/visible heavyweight-warning fact producers through shared commands, so the pack can resolve and test its real rival facts before M3-02 presents them. Full M4 rival planning is outside the slice. Random world coverage starts with a small business-demand incident, not a new global disaster or political simulation. Geography/hazards, elections/policy and conditional future developments add providers/definitions with their owning M5–M7 systems. The evaluation response, supplier offer and recovery offer provide three explicit opportunity/recovery paths; at full coverage, retain the existing one-third minimum. A normal session need not force every random incident to occur; matched seeded fixtures must demonstrate each.

The slice must remain completable with optional story embellishments disabled in a fixture. Retain tutorial instructions, required decisions and essential warnings. This check establishes that human enrichment adds interest without carrying missing core mechanics; it does not waive the E1 authoring or human playtest gates.

## Detection, dispatch, and presentation

1. A system publishes a typed fact from a committed transaction or an eligible scheduled/sampled occurrence. Record the source receipt/occurrence ID, date/phase, scope, visible result and cause. Speculative previews do not publish authoritative facts.
2. Registered detectors observe changed facts and due-time boundaries. Their subscriptions, required facts and evaluation phases are explicit. Threshold detection distinguishes an entry from a condition that remains true; duration checks use saved start/reset state and the defined daily fallback.
3. Match definitions in the shared stable order and reserve an occurrence key based on the source occurrence, semantic fact and scope. Keep consumed keys/counters across save/load and content migrations. A definition revision alone cannot reset a once-only occurrence. Story overlays and baseline presentations reference the same occurrence.
4. Shared flows validate and commit authorized effects. End-of-day reactions queue next-opening work. An explicit opening-due occurrence uses opening semantics; no provider rewrites already completed work or repeatedly seeks a same-phase fixpoint.
5. Build player-visible items from the committed result and the player's knowledge. Essential decisions retain their real simulated deadlines. Routine milestones and world headlines go to a grouped digest; duplicates may share a summary with a count and source references. Grouping presentation must never suppress a required decision or drop an unresolved authoritative occurrence.

Random rules specify eligible era/scenario/scopes, integer probability or weights, cadence, cooldown, maximum/overlap bounds, magnitude/duration limits, resource source and expiry. No-event outcomes are explicit. Use the existing owning hazard/market/etc. stream where the system already samples; narrative-only branches use the narrative/content stream. Never draw again to produce its headline or to reopen a pending occurrence. Save the selected outcome and captured bindings before exposing it. Unrelated presentation changes must not consume authoritative draws.

Inspection uses copied state/RNG and returns clause values, selected targets, path, effect receipts and future queues. Player forecasts report ranges/eligibility without exposing the copied future draw. Randomness and hidden information are not an excuse for a misleading causal explanation.

## Adding systems and content

Use small compiled modules with explicit dependencies; do not create an executable-plugin interface. Each system registers stable namespaced facts and permitted effects with type, scope, unit, availability phase, help metadata and tests. It publishes committed facts through the common boundary. The content loader compiles baseline rules and story layers into the same bounded normalized flows; the UI reads their projections rather than maintaining an event-type switch for every new story.

New definitions using existing operations are data changes. New mechanics require a tested core operation and its registry metadata. Unsupported required facts/effects fail validation; an optional pack is enabled only with its declared system dependencies. No missing reference silently becomes false or runs a substitute effect.

An extension is ready when it includes:

- Stable system/fact/effect/definition IDs and explicit required systems/schema/rules versions; no balance values hidden in QML.
- Typed input/output and knowledge-visibility contracts, evaluation phase, occurrence identity, deterministic ordering and saved RNG/continuation requirements.
- Validated definitions/localization, opportunity/recovery coverage, and explicit presentation replacement versus follow-up behavior.
- Positive/negative/threshold fixtures, duplicate suppression, resource/ledger effects, save/reload while pending, expiry/cleanup and bounded queue/repetition evidence.
- Revision/hash/content-version changes and a migration or a safe unsupported-save error for changed pending occurrences. No reset of old cooldowns/receipts by replacing IDs.
- The same Studio browsing, forms/graph, trace and test path as existing content. Built-in, AI-assisted and human-authored entries have equal validation requirements.

Build only the interfaces demanded by the next system. This contract does not authorize dynamic plugins, player mods, a universal framework or an additional scripting language.

## AI implementation assignments

The ordered [backlog](BACKLOG.md) owns task completion and dependencies. [The implementation brief](../agents/tasks/base-events.md) makes these assignments concrete:

- **M1-07:** Implement base-event detection, occurrence dispatch and truthful news projection after the shared runtime and save system. Test causal receipts, threshold edges, overlay deduplication, persisted random outcomes, digest priority and bounded queues.
- **M3-05:** Create and validate the eight-definition baseline after the slice mechanics and shared forms/inspection exist. Supply bounded rival/visible-warning producers through shared commands. Coding agents author the rules/templates/fixtures and inspect them through Studio. Internal snapshot/version finalization follows E1A-04/05 after M3-03 in the current Mac queue. Attach richer story treatment selectively. Run a seeded headless demonstration and pending-occurrence reload checks.
- **M5–M7 / M8-01:** Extend coverage alongside each implemented system, then audit the full base pack, opportunities/recovery, repetition and human story enrichment. Do not claim century coverage from the slice fixtures.

M1-07 now supports `on_committed` subscriptions for settled project completion
and missed contract starts. The next prepared opening resolves these sources
with the existing flow lifecycle. Saved activations carry the actual source
result and a saved cursor preserves pending work through choices. The inbox
projects this cause from the activation. [Committed-fact evidence](evidence/M1-07-committed-facts.md)

Scheduled and bounded interval roots can use the markets or hazards stream at
entry, with an explicit quiet outcome. Those samples share the existing graph,
rollback and save rules. The weekly digest groups ordinary headlines and keeps
decisions and active liquidity deadlines in the action tier. The eight-flow
playable pack now supplies concrete research/evaluation/pilot providers, real
supplier/recovery terms, demand expiry and bounded rivals. Its complete three-year
baseline and interrupted continuation match. [Implementation contract, limits
and seeds](../src/runtime/BASE_EVENTS.md). [Original seeded-incident evidence](evidence/M1-07-seeded-digest.md)
