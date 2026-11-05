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

An optional `variables` array declares at most 512 scoped story values. Each has
`id`, `scope`, `type` (`integer`, `boolean`, `text`), `unit`, `minimum`, `maximum`,
`default` and `help`, plus optional `visibility`. IDs cannot replace owning system
facts. Variables are facts with explicit defaults, so ordinary typed comparisons
can read them; sparse per-entity overrides are saved locally.

Expression values are `{"literal":2,"unit":"count"}`, `{"fact":"story.offers"}`,
`{"add":[left,right]}`, `{"subtract":[left,right]}` or
`{"scale":{"value":expression,"numerator":3,"denominator":2}}`.
Arithmetic requires integer matching units and uses checked operations; scale
rounds signed halves away from zero once. Trees are bounded to depth 16/128
terms and ratios to one million, with positive denominators. A comparison's
`value` may be an expression. Assignment uses
`{"op":"set_variable","target":"self","variable":"story.offers","value":expression}`
and requires the variable's declared scope/type/unit/range. Later refusal rolls
back prior assignments in the same operation. This is typed data, not scripting.

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
or nested `all_of`, `any_of`, `not`, `holds_for_days` or `related`. Comparisons are `eq/ne/lt/lte/gt/gte`;
boolean/text facts only allow equality/inequality. Scope, scalar type, unit and
range are checked without coercions. Missing vocabulary is an explicit error.

`holds_for_days` contains an object with a stable `id`, integer `days` in
1–1,000,000 and one nested `condition`. Predicate IDs are unique across a flow's
trigger and exit condition; another flow has its own namespace. Existing
condition depth/count limits include duration wrappers. Runtime uses elapsed
daily observations and persists resets; see the
[duration evidence](../../docs/evidence/M1-06-duration.md).

A related-entity predicate binds its `where` condition to each member of a
registered relation. This example requires exactly one active project:

```json
{"related":{"relation":"company.projects","match":"count","op":"eq","value":1,
            "where":{"fact":"project.is_active","op":"eq","value":true}}}
```

`match` accepts `any`, `all` or `count`. Only `count` takes a comparison and
integer threshold from zero through the relation's member bound. An empty
collection gives false for `any`, true for `all`, and a count of zero. The
foundation registers `company.capacity_sources`, `company.projects`,
`capacity_source.projects` and `company.contracts`; members include historical
rows where applicable. Use member facts to filter by active or completed state.
Nested selectors follow declared owner/member scopes. A duration may wrap
the aggregate and keeps the flow owner's daily history. Member predicates
cannot declare their own timers. Effects and lifecycle ownership stay on their
existing explicit scopes.

An optional `on_committed` on a company flow selects a registered settlement
source such as `capacity.project_completed` or `market.contract_missed_start`.
The authored trigger is an additional prepared-morning guard. Use `once` or
bounded `recurring` with cooldown. Each scenario can enable one root per
source. The root can call ordinary reusable story subflows, and cannot itself
be called as a subflow. The settled entity, date and typed owning result remain
attached to the activation through choices, delays and save/reload.

Lifecycle modes are `once`, bounded `recurring`, bounded anchored `interval`,
`scheduled`, and `continuous` with an explicit exit condition/node. Intervals
accept days/calendar months and campaign_start/scope_created/month_start/fixed_date
anchors with explicit offset and month-end clamp policy. These are typed
contracts; date scheduling and lifecycle execution belong to M1-06.

Nodes are `end`, `effect`, `news`, `dialogue`, `result`, positive-day `delay`,
ordered `sequence`, `subflow`, or weighted `random`. A random node requires
a registered `stream` and 2–32 ordered `branches`, each with a unique `id`,
positive integer `weight` (at most 1,000,000) and explicit `next`. Narrative
branches use `narrative`. The `markets` and `hazards` streams are available only
at the entry of a scheduled or bounded interval root. That entry cannot be
called as a subflow or revisited by a graph edge. Its explicit `no_event` branch
goes directly to an `end` node. All other branches begin a consequence path.
The occurrence limit counts sampling reviews, including quiet outcomes.
Every outcome participates in graph, cycle, sequence-region and
continuation-depth checks. Other source streams remain unavailable.
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
checked `register_system`, `register_fact`, `register_effect` and `register_relation`; dependencies
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
