# First-playable base events

The canonical internal development collection is
`data/runtime/first-playable.json`. Its eight `slice.*` flows use the existing
compiler, lifecycle/continuation runtime, causal receipts, choices and weekly
digest. English messages are versioned collection data. The collection remains
`0.0.0-dev`; internal snapshot finalization and shipped content are later gates.

`committed.cpp` supplies actual lab research completions, independent evaluation
findings and first/final/failed pilot settlements. Sources capture the settled
result, subject and date. A later successful delivery cannot rewrite a saved
zero-delivery warning. Source identities include the settlement date, allowing
distinct outcomes from the same contract. Explicit committed-source flows may
use a zero cooldown to retain multiple same-day results; ordinary recurring
conditions still require a positive cooldown. All occurrence counts remain bounded.

`core/business_offers` owns the disclosed offer catalog and accepted terms.
The supplier adds 30 CU/day for 180 days, $1,000 setup and $4,000 per calendar
month including power. A new source records its first available day; old source
history is not backfilled. Its exclusive expiry stops capacity and the recurring
obligation. Decline creates no commitment; acceptance rechecks protected cash,
capacity, customer eligibility and existing obligations. The document-only
recovery pilot creates a real 500-task/day contract for $45,000/month over 90
days, with fees/credits based on actual work. Opening it grants no capital.

External demand effects retain an occurrence-owned dated change. The slice
uses ±20% for 30 days with explicit quiet outcomes and 90-day reviews. Customer
budgets and existing signed tasks are protected. Modifiers cannot overlap for
one segment, rewrite settled demand or keep changing demand after expiry.
Their records remain available for history validation and causal inspection.

`competition.cpp` maintains two minimal startup routines and one heavyweight.
Each owns ordinary accounts, paid staff/compute, licensed data, finite training
and independent saved streams. Actions run through the same core commands as
the player. A two-month buffer guards new work; unfunded routines decline and
can close through real liquidity policy. No capital, research or hardware is
created by a notice. A heavyweight observes actual customer delivery, discloses
a warning, waits at least 90 days, then funds its bounded experiment. Private
player training and private rival scores do not become public facts. The slice
does not implement M4's full market/research planner or replacement entrants.

Player and rival daily commands reuse an enclosing copied transaction instead
of copying full history again. Checked daily systems validate their appends;
save/load and external checkpoint validation still replay complete histories.
The standalone core command API retains independent atomic rollback. The
internal `submit_in_transaction` API requires its caller to discard a refusal.

The engineering baseline runner starts without a product, trains/evaluates,
uses authored service/contract terms, then retrains with targeted data and
independently verifies the second service. It disables only optional story
roots. Its deployment helper exercises existing business commands after checking
evidence; atomic player deployment controls, presentation and tutorial remain
M3-02. It is not a player standing-policy implementation or a complete playable UI.

Run:

```sh
build/release/bin/pgp-headless --events-demo data/runtime/first-playable.json
ctest --test-dir build/release -R '^runtime.base-events$' --output-on-failure
```

Seed `20140101` finishes 2017-01-01 with 229,865,809 constant-2014 cents,
4,001,600 useful tasks, four competitive notices and no new financing. The
warning occurs on ordinal day 64 and financed work starts on day 154. Private
rival seeds are 4101/4102/4103. Focused first-draw fixtures use supplier seeds
8 (offer) / 0 (quiet), and demand seeds 1 (increase), 8 (decrease), 0 (quiet).
The test derives these deterministically with the core stream API, exercises
all eight families, saved pending choices/expiry/mobilization, unaffordable
and non-firing boundaries, source deduplication, Studio browsing/typed references
and copied impact inspection. Full uninterrupted/reloaded baseline states match.
