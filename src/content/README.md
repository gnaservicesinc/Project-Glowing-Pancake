# Foundation content schema 1

`pgp::content` is the Qt-free content adapter, above `pgp::core`. Its public
headers contain typed definitions and registry metadata, with no JSON-library,
filesystem or network types. `load_collection(bytes, registry)` returns a fully
validated collection or a `ValidationError` containing a stable code and path.
It performs no effects or resource loading.

The complete supported envelope is:

```json
{
  "content_schema": 1,
  "content_version": "0.0.0-dev",
  "currency_basis": "constant_2014_cents",
  "definitions": []
}
```

At least one definition is required. The positive schema integer, SemVer content
identity and each positive definition revision have distinct meanings. Loading
neither finalizes content nor changes `project-versions.json`. The canonical
engineering example is
[`tests/content/fixtures/foundation.json`](../../tests/content/fixtures/foundation.json).
It is outside game resources and is not a reviewed playable pack.

Each definition has `id`, `kind` and `revision`. Current typed payloads are:

| Kind | Required payload fields |
| --- | --- |
| `scenario` | `starts_on`, `ends_before`, `seed`, `cash_cents`, `capacity_milli_CU_per_day`, `lease_energy_included`, `team_people`, `headquarters_choices`, `ownership_basis_points`, `monthly_commitments_cents`, `research`, `flows` |
| `research` | `prerequisites` (`all_of`, `any_of` reference arrays), `cash_cents`, `staff_days`, `work_milli_CU` |
| `deal` | `minimum_payment_cents`, `minimum_duration_days` |
| `flow` | `scope`, `trigger`, `lifecycle`, `entry_node`, `nodes`; optional `exit_node`, `max_node_visits`, `tags` |

Money/work/rate/time/share fields become the existing strong core units.
Ownership totals exactly 10,000 basis points; commitment sums must fit cents.
Scenario research/flow and negotiation-template references must exist and have
matching kinds. Prerequisites are acyclic. Dates use the actual Gregorian
calendar, including non-leap 2100. The initial headquarters vocabulary is US-CA,
US-TX and US-WA. Additional scenario geography is a later schema extension.

Conditions are registered fact clauses (`fact`, `op`, `value`, optional `unit`)
or nested `all_of`, `any_of`, `not` or `holds_for_days`. Comparisons are `eq/ne/lt/lte/gt/gte`;
boolean/text facts only allow equality/inequality. Scope, scalar type, unit and
range are checked without coercions. Missing vocabulary is an explicit error.

`holds_for_days` contains an object with a stable `id`, integer `days` in
1–1,000,000 and one nested `condition`. Predicate IDs are unique across a flow's
trigger and exit condition; another flow has its own namespace. Existing
condition depth/count limits include duration wrappers. Runtime uses elapsed
daily observations and persists resets; see the
[duration evidence](../../docs/evidence/M1-06-duration.md).

Lifecycle modes are `once`, bounded `recurring`, bounded anchored `interval`,
`scheduled`, and `continuous` with an explicit exit condition/node. Intervals
accept days/calendar months and campaign_start/scope_created/month_start/fixed_date
anchors with explicit offset and month-end clamp policy. These are typed
contracts; date scheduling and lifecycle execution belong to M1-06.

Nodes are `end`, `effect`, `news`, `dialogue`, `result`, positive-day `delay`,
ordered `sequence`, `subflow`, or weighted `random`. A random node requires
`stream: "narrative"` and 2–32 ordered `branches`, each with a unique `id`,
positive integer `weight` (at most 1,000,000) and explicit `next`. An outcome
linked to `end` represents no event. Every outcome participates in graph, cycle,
sequence-region and continuation-depth checks. Other source streams are refused
until their owning systems provide the appropriate occurrence semantics.
Presentations store localization keys;
illustrative draft prose and saved-instance annotations are not runtime fields.
Each dialogue has unique choices and an existing dismissal default. Every node
is reachable from entry or the continuous exit. Node links and subflow references
are checked. Reusable graph calls and trigger dispatch share definitions; M1-06
must implement activation semantics without creating a second automatic
activation for an incoming call.

Every effect has explicit `op` and `target` (`self` or `world` in this initial
binding vocabulary), then only its registered typed arguments. Optional `units`
map argument names to their registered units. Targets must match allowed scopes.
`open_negotiation.template_id` is a checked `deal` reference. Modifier fields use
registered choices; strings never become expressions. Facts/effects have typed
ports, help, units/ranges, scopes, phase boundary, knowledge visibility,
provider and dependency metadata. Engine modules extend a `Registry` through
checked `register_system`, `register_fact` and `register_effect`; dependencies
must already exist, so registration cannot create cycles. Registry entries
describe available semantics; registration alone does not execute an operation.

Immediate node cycles and recursive subflow calls are refused. Delayed graph
cycles require a positive simulated delay and explicit finite `max_node_visits`.
Continuation depth, including sequences and subflow calls together, is at most
eight. Positive-weight cycles that would accumulate frames are refused even
with a delay. Depth is computed by bounded reverse-edge propagation, so shared
converging branches do not enumerate exponentially many paths.

Input budgets: one MiB, JSON nesting 48 and 100,000 parser events; at most 2,048
definitions, 512 nodes per flow, 64 effects/sequence children per node, 32 choices
per dialogue, and 128 condition terms with nesting 16. All JSON object keys must
be unique. Fraction/exponent numeric tokens, unsupported schemas/kinds, unknown
fields/operations, numeric booleans and implicit unit conversions fail. Strict
field and operation vocabularies provide no executable script, remote-resource
or arbitrary path mechanism. Ordinary text is not executed.

This initial foundation is deliberately a small typed contract. Dataset,
hardware, recipe, product, market, policy, rival, broader world/objective and
asset/localization catalog schemas arrive with their owning mechanics; they
currently fail `unsupported_kind`. Their fields are not accepted as unvalidated
opaque objects. Research here validates resource/prerequisite definitions, not
the complete M3 research mechanics. The deal record is negotiation-reference
metadata, not a signed contract. The [runtime adapter](../runtime/COMPILER.md)
now compiles and executes this schema with timing, traces, receipts, owned
modifiers and effect dispatch. The [persistence adapter](../persistence/README.md)
loads exact saved state. Broader flow-language extensions remain M1-06 work.
Canonical definition identity exists; full shipped asset/localization collection
hashing/finalization and editor IO remain later authoring/packaging tasks.
