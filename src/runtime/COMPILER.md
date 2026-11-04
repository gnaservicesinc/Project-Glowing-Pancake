# Shared compilation and inspection boundary

`compile_collection(collection, registry, available_effects)` validates the typed
collection even when a caller assembled or edited it in memory. It checks
incompatible typed payloads, normalizes every supported field, and reuses the
content loader's schema/reference/cycle/unit/depth validation. An explicit
executor vocabulary rejects operations that have metadata but no handler.
Compilation performs no effects, file IO, random draws or state advancement.

The compiler retains the normalized typed collection, registry and ordered
diagnostics. `waiting_sibling` is an informational diagnostic: a delay or choice
in an early sequence child suspends later children until that child returns.
The compiler uses graph reachability rather than enumerating all possible paths.
Eight continuation frames are legal; a ninth is rejected by shared validation.
Sequence children cannot jump into sibling effect/presentation paths or a
nonterminal parent return path. Shared terminal ends remain legal. Region
analysis has a deterministic one-million-visit guard.

`canonical_content_bytes` is deterministic JSON for the supported definition
collection. Definition IDs and map keys are sorted. Source whitespace, object
key order and definition-array order do not affect it. Ordered effects,
sequence children and dialogue choices retain their order. Revisions, text
keys, timing and effect parameters are included. The identity covers definitions
only: there is no shipped asset/localization catalog in this foundation yet.
Persistence hashes these bytes separately from build/source identities.

`evaluate_condition` validates every typed clause and its actual provider value,
then reports authored-order paths, matched scope, unit, comparison, actual and
expected values, and truth. Boolean/text values permit equality/inequality only;
there is no numeric or unit coercion. The reader callback must be a pure query.
No RNG or mutable-state handle is accepted by the inspector.

Author-fixture mode exposes full fixture truth. Player-known mode does not even
read a fact marked `hidden_truth`; its value, expected threshold and clause
truth remain absent. Aggregate truth is three-valued: it remains unknown when
hidden clauses determine the outcome, while a publicly decisive false `all`
or true `any` can still be explained. This metadata filter complements the
owning reader's scope/access checks; it does not grant access to another entity.

`resolve_anchor` returns an explicit first due date and preferred calendar day.
The runtime saves both. `interval_due` computes each date directly from that
anchor rather than the last clamped result, so January 31 → February 28 →
March 31 does not drift to the 28th. Month-start means the first boundary on or
after scope creation, followed by the authored offset. Dates stay within the
core's Gregorian years 1–9999 and checked integer interval bounds.

The compiler-specific executable initially passes 62 behavioral checks under
Apple Clang with warnings treated as errors, including additional `-Wshadow`.
The integrated implementation log records the final suite and native results.

The executable vocabulary also includes explicit narrative-stream weighted
random nodes. Canonical identity retains ordered branch IDs, weights and edges;
compilation and pure condition inspection consume no randomness. Execution and
exact saved outcomes are documented in the
[weighted-branch evidence](../../docs/evidence/M1-06-weighted-branches.md).
Daily duration predicates also compile and execute through the
[saved temporal contract](../../docs/evidence/M1-06-duration.md). Pure inspection
accepts a read-only observation reader/date and emits start/elapsed/result
traces. A missing reader/date is unknown; explicit absent history is unsatisfied;
stale samples are unknown. Hidden descendants redact temporal history without
reading it. Current fact clauses and cached duration truth are distinct outputs.
Related-entity selectors/quantifiers, registered variables and checked expression
trees still require shared schema/compiler/runtime extensions. M1-06 remains
open for those forms and its prerequisite gate.
