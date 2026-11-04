# M1-06 daily duration predicates

Implemented and locally verified 2026-11-04 against baseline 1.2. This bounded
slice connects typed duration conditions, daily observation, pure traces and
exact save/resume. M1-06 remains open for registered variables/expressions,
related selectors/quantifiers and prerequisite platform acceptance.

## Authored and daily contracts

```json
{
  "holds_for_days": {
    "id": "sustained_operating",
    "days": 2,
    "condition": {"fact": "company.is_operating", "op": "eq", "value": true}
  }
}
```

The ID is unique across one flow's trigger and exit condition. Other flows have
their own namespaces. Days are integer tokens in 1–1,000,000; exactly one child
condition is required. Existing depth-16/count-128 condition budgets include
duration wrappers. Canonical definition bytes retain ID, days and nested
condition; existing definitions without duration retain their canonical bytes.

First observed true on D starts at zero elapsed days and satisfies N on D+N.
A false sample clears the start. A gap between observed dates restarts a true
run instead of crediting unseen days. Nested durations observe inner before
outer. Negation and boolean all/any never suppress sampling/reset of a child.

All enabled roots' trigger and exit predicates observe the same opening world
after core deliveries/choices and before any content effects. They update even
while a flow is active, on cooldown, off interval or out of occurrences. Disabled
reusable flow triggers acquire no autonomous history. Exit timers measure
scope condition history, including before activation and across reentry; they
do not measure activation age. Existing continuous lifecycle evaluation still
controls when an eligible exit runs.

Samples freeze for the morning through content effects, delays and choices.
A cash change after the prepass is observed tomorrow. Repeated preparation,
validation and inspection neither advance timers nor initialize them. No RNG
stream is consumed. Observation/inspection work shares the 4,096-step operation
budget; refusal rolls back all candidate timers, date changes and consequences.

## Persistence and inspection

Each `DurationState` retains definition/revision, predicate ID, typed binding,
last observed day and optional true-run start. A missing start is the saved
false/reset state. `duration_sampled_day` records the completed prepass even
when an opening later suspends at a required content choice. Histories are
canonical, unique and complete for enabled timed roots from the actual program,
rather than only roots still present in the save. Each timed root must retain
its lifecycle owner. Dates must match the cursor and their scope's creation.

Prepared/work-ready mornings and content choices require today's cursor. Raw
core opening/choice boundaries and finished scenarios may retain an earlier
cursor; a later preparation handles any observation gap explicitly. Dropped
rows/lifecycles, a lost cursor, wrong definitions/scopes/revisions, future dates,
start-after-observation and stale prepared samples fail strict restore.
This validates structure and continuation, not authenticity or a replay of all
historical facts. Coordinated falsification of otherwise valid history dates
is not an authentication claim.

The pure inspector accepts a read-only temporal reader and inspection date.
Missing context is unknown; explicit absent history is not yet satisfied;
stale observations remain unknown. Traces show required duration, sampled day,
start, elapsed days and cached truth alongside current fact clauses. Current
facts can differ after an effect without changing the saved daily sample.
Player inspection with any hidden descendant redacts history and result and
never invokes that history reader; author fixtures can inspect full test state.

Development content schema/save format remain 1. New saves include the paired
`duration_states` and `duration_sampled_day` fields. Older format-1 saves may omit
both only when the entire loaded collection has no duration predicate, including
disabled definitions. Prior random-history compatibility is preserved. Older
readers reject the new fields safely; no product release or migration is claimed.

## Observed verification

Existing arm64 Mac, Apple Clang 21.0.0, C++20 and warnings treated as errors:

| Check | Result |
| --- | --- |
| `./make.sh --core-only --test --jobs 2` | Release, 27/27 CTests pass |
| `./make.sh --debug --core-only --test --jobs 2` | Debug, 27/27 CTests pass |
| `pgp-content-test` | 206 checks pass |
| `pgp-compiler-test` | 147 checks pass |
| `pgp-duration-test` | 119 checks pass |

Fixtures cover D+N/reset/gap boundaries, nested and negated durations,
off-interval/cooldown/exhausted roots, sampled-before-effect cash, a saved
required dialogue, natural continuous exit/reentry and unchanged RNG. Saving
immediately before a threshold produces an identical next-day checkpoint.
Later effect failures and an over-budget 2,079-predicate prepass preserve the
entire prior checkpoint. Rechecksummed corrupt saves exercise missing/backdated
history/cursors, wrong ownership/dates/types, duplicate rows and legacy input.
Pure author/player traces verify cached/current distinction and hidden-history
redaction without reads or mutation.

Native CI results are recorded after observation in the implementation log.
No graphical launch, player-facing business scenario, package or release was
created. The next bounded flow slice is registered scoped variables/flags and
checked value expressions; related entities require real owning state later.
