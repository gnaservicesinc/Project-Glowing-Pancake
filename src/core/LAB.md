# First-playable laboratory

`lab.hpp` owns delivered data licenses, research attempts, checkpoint lineage and
evaluation evidence. Scenarios supply a validated catalog; the shared compiler
hash includes every numeric input. The first-playable catalog is
`data/runtime/first-playable.json`: four fictional data offers, six research
nodes, five recipes and three evaluation plans. No real dataset or ML execution
is included. Fictional use rights have no bearing on the game's asset licenses.

All mutations use revision-checked `Simulation` commands and the existing
accounts/capacity transaction. Data and methodology costs are paid once per
accepted purchase/attempt, with no refund on cancellation. Training pays no
second compute charge for the leased capacity. Research requires both the
authored minimum experiment work and enough allocated experiment time for its
staff-days. A cancelled research attempt can restart with a new paid attempt.
Serving retains the capacity system's existing priority during shortages.

Training implements the simulation specification's single-round effective-data
and complementary saturation formula, in total milli-CU and basis points. A
model stores actual accumulated work, the recipe and a sorted unique data
mixture. Cloning a stopped checkpoint preserves exactly those resources and
capability. Continuing adds only new actual work. Continuation retains its
recipe/mixture; using new data requires a fresh candidate in this slice, so old
compute cannot instantly learn newly purchased data. Cross-recipe transfer,
synthetic data and broader capability domains remain later work.

Previews return time, license cost, bottlenecks and uncertain intervals, without
reading/drawing RNG. They assume full requested capacity; real outages delay
actual work. The immutable report omits true capability and stream state.
Evaluation requires a stopped model, methodology prerequisites, staff, compute
and payment. On completion it draws the saved research stream, retaining the
draw boundary for exact validation. Independent tests report modeled uncertainty;
overlapping headline tests can be biased by contamination. Neither raises true
capability. Findings name only covered subgroups and suggest targeted retraining,
human review or narrower promises. M3-02 now wires those responses into authoritative deployment and the player interface.

Saves retain all licenses/payments, research attempts, actual work, lineage,
evidence and RNG. Strict loading reconciles the authored catalog, actual capacity,
payments, prerequisite timing, model formula and every evaluation draw. Older
engineering saves retain an absent lab; a lab scenario cannot silently drop it.
History/draw replay is confined to load/save validation, not daily advancement.

Run `build/release/bin/pgp-headless --lab-demo data/runtime/first-playable.json`
for a baseline finding and targeted recovery. `runtime.lab-integration` covers a
hand-calculated arithmetic case, malformed data/saves, paid unlocks/retries,
resource/payment failures, cancellation, outage recovery, clone/mixture exploits,
untested subgroups, narrower audit uncertainty and matched save continuation.


## Qualified deployment

`DeployModel` owns the service transaction: select an authored scope, stopped
checkpoint, completed independent evidence and source. Evidence must measure
that exact checkpoint's consumed work; its lower bounds must meet each promised
covered domain. All inherited data must permit commercial use. Training previews,
headline tests and untested domains cannot qualify a promise.

`ServiceDefinition` carries task requirements and optional review credits,
throughput, staff/support costs and customer preferences. Review changes service
qualification, never the model/evidence score. Serving uses the selected recipe's
actual inference cost and shares finite source/staff reservations. Setup refuses
atomically when evidence, rights, resources or an existing customer promise fail.
The baseline runner uses this same command.

Replacing a service cancels its prior serving/support profile and reserves a new
one in one transaction, retaining product identity, contract terms, earlier
capacity bindings, delivery and invoices. Ordinary cancellation requires clearing
active/pending customer commitments first. Full save validation reconciles the
authored catalog, exact evidence, dated serving profiles and support obligations;
none of that history is replayed inside the daily advancement path.

`StageServiceRollout` qualifies a candidate through the same evidence and rights
checks, then reserves a smaller dated serving project. It uses the live product's
territory, quote and demand. Each buyer's settled discretionary work can route up
to the chosen share to the candidate if its actual resources and eligibility allow
it. Signed work always routes to production. The two workloads reconcile to one
customer charge. Customer-hosted candidates keep the current hardware footprint.
Support is prorated to the candidate's daily task cap and ends with the trial.

`FinishServiceRollout` discards the candidate or promotes it after actual trial
delivery. Promotion reserves a full production profile and rechecks signed terms
and current launch rules. At the deadline the trial reservation ends and the
decision stays available. `RollbackService` records a new production binding to
the previous distinct model, evidence and review configuration on its retained
source, subject to the same qualification and resource checks. Release history,
settled work, invoices and signed terms remain available after either decision.
An active geographic replica must be stopped before staging or restoring a release.

`runtime.deployment` covers qualification, commercial rights, paid review,
resource refusal/rollback, contract protection, replacement, clones, forged
history and exact saves. `bridge.management` checks player options, amount
parsing, preview purity, public evidence/control terms and stable selection.
