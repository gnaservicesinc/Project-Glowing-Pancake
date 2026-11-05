# Implementation brief: playable base events

## M1-07 — Built-in event detection and news

- **Role:** [simulation](../roles/simulation.md), with [content](../roles/content.md) and [QA](../roles/qa.md).
- **Read first:** [event contract](../../docs/EVENT_SYSTEM.md), [simulation phase/runtime rules](../../docs/SIMULATION_SPEC.md), [technical data boundary](../../docs/TECHNICAL_PLAN.md), actual shared registry/flow/save APIs.
- **Dependencies:** M1-06 shared flow runtime and M1-04 phase-aware saves complete. Use the existing primitives/transaction APIs; do not bypass them to make a separate event engine.
- **Outcome:** Compiled system providers publish typed committed facts; detectors turn meaningful changes or bounded scheduled/seeded occurrences into deduplicated flow activations and truthful news. Definitions supply thresholds, probabilities, cooldowns and templates through versioned validated data.
- **Owned interfaces:** Minimal core fact/occurrence/detector registration and news projection, registry metadata, save fields/migration and focused tests. Choose paths to fit the implemented core; no central switch containing every story.
- **Acceptance:** One threshold entry fires once, re-entry follows its declared rule, one source receipt produces one consequence despite a story overlay, save/load preserves pending outcome/occurrence/cooldown, copied previews leave live state/RNG unchanged, unauthorized effects fail atomically, and a grouped digest never hides a required deadline.
- **Verification:** Exact seeded next-step state/receipt parity across uninterrupted and reloaded runs; positive/negative/boundary and malformed definition tests; deterministic bound failure with recoverable trace. Record actual architectures tested.
- **Outside scope:** Production narrative, new hazards/politics, runtime AI/news APIs, executable plugins, player mods, publication.

## M3-05 — AI-assisted playable base-event pack

- **Role:** [content](../roles/content.md), with simulation/QA and Content Studio briefs.
- **Dependencies:** M3-01 slice mechanics, M1-07 event detection/news and the shared compiler/forms/inspection path. M3-02 presentation consumes this pack. E1A-04/05 snapshot finalization follows M3-03 under the current Mac-first queue; M3 playtest/E1b gates still apply.
- **Outcome:** Coding agents create the eight baseline flows in [the coverage matrix](../../docs/EVENT_SYSTEM.md#first-playable-coverage), localizable headlines/results, inspectable causal traces and scenario fixtures. Include the two minimal budget-bounded rival routines and visible heavyweight-warning fact producers through shared validated commands; M3-02 presents these, while full rival planning remains M4. Human-authored stories augment selected occurrences; manual human creation of every base event is not a prerequisite.
- **Acceptance:** All eight definitions resolve real facts/operations and validate. Evaluation-linked existing data/retest options, the supplier offer and the recovery offer supply three explicit opportunity/recovery paths with real eligibility/costs. Explicit seeds exercise technology, evidence, news, finance/rival responses and a random business-world incident; no random production run is required to show them all. Effects, deadlines and text agree. A playable baseline fixture works with optional embellishments disabled. Required tutorial/warnings/recovery remain. No duplicate consequence, free contract, hidden future draw or unbounded repeated alert.
- **Verification:** Inspect the pack through the shared Studio document/probe path; run positive/negative/threshold/choice paths and saved pending occurrences; compare uninterrupted/reloaded outcomes. Record exact source/content hash/version and seeds. Internal snapshot/version finalization remains E1A-04/05, and balance/player comprehension remain M3-04 evidence.
- **Outside scope:** More than the eight-definition slice budget without a recorded scope change, full politics/disasters/century simulation, live news, generated runtime prose, remote AI, release publication.

Task state and handoff belong in [backlog](../../docs/BACKLOG.md), [status](../../docs/STATUS.md) and [implementation log](../../docs/IMPLEMENTATION_LOG.md). This brief assigns future work; it is not evidence that either task is complete.
