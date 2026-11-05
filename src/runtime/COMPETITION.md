# Planned competition

M4-01 adds weekly company strategies above the Qt-free core. A scenario's typed
`competition` setup selects bounded slots from authored `rivals[].planning`
profiles. The standard roster schedules six domestic and six international
startups plus three heavyweights. Three startups in each group enter initially;
later slots enter at authored dates. Reduced (7), standard/forgiving (15) and
expanded (20) presets are distinct, saved scenario identities. The original
three-rival introduction retains its smaller campaign rules.

Scheduled companies own no cash, staff, compute or bills until their actual
entry. Opening capital, payroll, leases, research portfolios, priorities,
reserve/runway, price limits and publication choices come from validated data.
Heavyweight profiles with a `response` policy use the public-evidence pressure,
90-day warning, paid mobilization, delivery and withdrawal rules in
[HEAVYWEIGHTS.md](HEAVYWEIGHTS.md). Profiles without that policy retain the
original player-delivery warning routine; the two triggers cannot overlap.

Each operating company consumes one private saved rivals-stream draw per weekly
review. A response program starts those reviews only after its paid delivery
boundary, while competing; consolidation suspends new strategic work. It scores
a bounded feasible list of data, research, training,
evaluation, qualified deployment, price, hire, financing/repayment, paid compute
expansion and publication actions. Stable score/action/subject ordering breaks
ties. Refusals remain atomic. Decisions retain their cause, observed information
and RNG boundary; ordinary daily settlement moves state without copying or
revalidating its entire history. Closing companies stop operating and do not
receive replacement capital.

A strategy reads its own private ledger, commitments, work and evidence, plus
last-settled public product releases/prices and a seeded bounded estimate of
public customer demand. It cannot read private player cash, licenses, models,
contracts or future RNG. Historical observation validation recomputes those
public snapshots without drawing from live streams. Repricing preserves old
quotes/invoices and signed contract terms. A company's own recent deliveries
and recurring costs bound its viable price; they do not guarantee profit.

All organizations use the same core commands and eligibility/accounting rules.
`competition_market.cpp` obtains real contract/capacity opportunities, protects
signed work and allocates one shared customer task/budget pool with the existing
integer allocator. Owner projections and the same bounded latest industry-adoption snapshot pass
back through core settlement checks. This preserves one-basis-point Hamilton
rounding differences between industry shares and a local aggregated outside
bucket. Historical checks also enforce those actual industry migration caps.
Monthly installed shares are derived from actual industry deliveries and the
true outside option; each company's local report includes other providers in
its outside total. Saved history is checked for duplicated tasks/budget,
changed customer terms, missing first deliveries and forged public transitions.

Research publication is explicit and requires completed research. After the
setup's 30-day delay, a genuine paper can grant one 25% finite experiment-work
credit to that research node. It never completes research, bypasses prerequisites,
reduces cash costs or stacks duplicate credits. Player publication/adoption and
price controls use the same commands. Actual public papers and released prices
are visible in the Lab/Office; private rival continuations are not exposed.

Catch-up funding defaults off. The forgiving preset discloses one optional
$300,000 equity offer for 15% ownership per rival. It uses the actual financing
ledger and dilution rules and cannot be repeated. No periodic subsidy or
replacement company is added.

The save codec retains full company ledgers/work records using bounded lossless
row/string-table storage; see [persistence](../persistence/README.md). Exact
pending entry, active planning, publication-delay, insolvency and malformed-save
cases live in `tests/runtime/competition_test.cpp`. Full preset demonstrations
use `pgp-headless --competition-demo FILE PRESET DAYS`; their output states the
actual stop date and whether the company reached the end or a real warning.
The native `--competition-smoke` mode drives the actual QML controls in fresh
explicit portable storage. These are engineering checks, not human sessions or
release qualification. Century balance, long-run entry/consolidation and save
scaling remain M8 work.
