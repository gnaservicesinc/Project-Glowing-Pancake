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
human review or narrower promises. The latter two deployment responses and
contract qualification are wired by the later playable-presentation task.

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
