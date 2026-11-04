# M1-06 weighted narrative branches

Implemented and locally verified 2026-11-04 against baseline 1.2. This bounded
slice adds weighted branches to the shared content/compiler/runtime/save path.
M1-06 remains open for duration predicates, registered variables/expressions,
related selectors/quantifiers and its platform prerequisite.

## Authored contract

```json
{
  "kind": "random",
  "stream": "narrative",
  "branches": [
    {"id": "offer", "weight": 3, "next": "offer_delay"},
    {"id": "no_event", "weight": 7, "next": "end"}
  ]
}
```

Every random node has 2–32 ordered outcomes with distinct stable IDs and
positive integer weights up to 1,000,000 each. Total weight is bounded to
32,000,000. Only the saved narrative stream is supported; an explicit branch
to an end node supplies a no-event outcome. Other stream names, fractions,
zero weights, missing targets, duplicate IDs and incompatible payloads fail.

All outcome links participate in reachability, immediate/delayed cycle checks,
sequence-region isolation, eight-frame analysis and early-sibling wait lints.
Canonical definition bytes include branch order, IDs, weights, stream and
targets. Compilation and condition inspection perform no random draws.

## Execution and exact continuation

An executed visit samples `SplitMix64::bounded(total_weight)` once, using
rejection sampling and authored cumulative weight intervals. Rejection sampling
may consume multiple raw draws; the final stream state is retained exactly.
Only the selected edge executes. The committed trace identifies narrative
source, ordered weights, sample and selected branch. Failed operations label
their traces as candidate work and roll back RNG, IDs, receipts and effects.

Before following the edge, the copied transaction records activation ID, node
visit, date, flow/node, selected branch, sample and stream before/after. The
activation retains its bound scope and continuation frames. A later visit to
the same node through a delayed cycle is a new selection. Reopening a morning,
choice, result or saved delay does not repeat the prior selection.

The first selection captures `narrative_origin`, permitting an already-advanced
starting stream without assuming it still equals the scenario's initial seed.
Restore checks complete history against that origin, ordered visit identities,
dates, the root's reachable subflows, actual weighted selection, contiguous
stream transitions and the current narrative state. It replays draws on local
copies only. Saved continuation reachability follows the last selected edge,
refusing a delay/choice substituted from an unselected branch. These checks are
structural validation, not authentication or a replay of all campaign commands.
The narrative stream currently has no other runtime draw producer; future
occurrence sources must share its recorded transition boundary.

Development save format/content schema remain 1. New saves include both
`narrative_origin` and `random_resolutions`. Earlier deterministic saves can
omit both; random-bearing programs require both even before their first draw.
Existing definition hashes remain unchanged for unchanged deterministic content.
Older readers reject the new fields safely. No release content, product version
or semantic save migration is claimed.

## Observed local verification

Existing arm64 Mac, Apple Clang 21.0.0, C++20, warnings treated as errors:

| Check | Result |
| --- | --- |
| `./make.sh --core-only --test --jobs 2` | Release, 26/26 CTests pass |
| `./make.sh --debug --core-only --test --jobs 2` | Debug, 26/26 CTests pass |
| `pgp-content-test` | 174 checks pass |
| `pgp-compiler-test` | 77 checks pass |
| `pgp-random-branch-test` | 270 checks pass |

The new test uses a frozen independent integer oracle covering all ten sample
slots of 1:3:6 weights, including both sides of each weighted boundary. It also
checks an advanced stream origin and a crafted rejection-sampling case;
non-narrative streams remain unchanged. Save-before-selection and save-after-
selection fixtures compare entire checkpoints through nested sequence/subflow
delays, a required dialogue and the next day. Inner/outer effects execute once.

Further fixtures cover distinct visits in a delayed loop, visit-budget refusal,
stale/invalid commands, downstream unfunded-effect rollback and rechecksummed
malformed saves: wrong sample/outcome/stream/visit, orphan records, wrong node,
unknown/missing fields, fractional/narrowed integers, truncated history and an
unselected continuation. Failed loads preserve the live runtime.

Native CI results are recorded after observation in the implementation log.
No graphical app launch, business-economics scenario, native package or release
is part of these checks. The next bounded flow slice is persisted daily
duration start/reset semantics.
