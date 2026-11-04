# Illustrative data contracts

These JSON files are valid, hand-auditable design examples, **not production game content or a complete loader/schema**. M1-02 now supplies typed collection validation; the normalized [foundation fixture](../../tests/content/fixtures/foundation.json) adapts the opening balance and seven flow examples into that supported envelope. Resolve a changed schema deliberately rather than loading the prose examples directly.

- [Slice balance](slice-balance.example.json): opening units, recurring commitments and a training/contract example.
- [Supplier event](supplier-event.example.json): restricted trigger/effect vocabulary and a concrete negotiation opportunity.
- [Model report](model-report.example.json): capability estimates, separate reliability/control evidence, and limitations.
- [Flow lifecycles](flow-lifecycles.example.json): six news/hidden graphs using all five activation modes, including an owned continuous modifier, a relative anchor and nested sequence continuation state.
- [Trigger impact](trigger-impact.example.json): explicit effect targets, threshold boundaries, price direction/target expectations and deliberately wrong controls.
- [Simulation boundaries](simulation-boundaries.example.json): wide intermediate products, single-round basis-point math, rational budget scaling, serving capacity and exact objective windows.

Dates/names/statistics are fictional game data. No external resources or executable expressions are loaded by these examples. Per-field units are named explicitly. All `_cents` balances/thresholds/prices use constant-2014 purchasing power; `milli_CU` work and `milli_CU_per_day` rates are distinct. Every normalized effect item names its target; no omitted runtime target defaults. Relative interval anchors are resolved once and saved. JSON numeric integers represent stored units, not dollars or percentages unless the field says so.

Expected assertions here describe design expectations. Selected vocabulary/deal
references are now registered and tested in M1-02's normalized fixture; this
does not execute effects or establish flow/save/business runtime evidence. The
root `project-versions.json` is separate initialized project metadata; the six
files here remain example-only.
