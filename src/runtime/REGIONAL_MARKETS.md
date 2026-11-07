# Regional markets and wholesale compute

Use **Lab → Deploy** to select a channel, qualified checkpoint and launch
territory. Each service keeps its territory when you replace its model.
**Office → Regional markets** shows each of the eight regions, the latest
settled day, total useful tasks and your share of that work. Select a region
to see its six customer groups, remaining demand and customer hardware use.
The profiles are fictional 2014 game assumptions.

The launch catalogue adds two templates each for business contracts, developer
APIs, consumer applications, on-premise licensing, open weights and agent
services. It retains the introductory document services. A template defines
its evidence, research, support, throughput, customer fit and migration limits.
Agents require a paid review team. Open weights require redistribution rights
for the full training lineage and carry a zero task price. The current Lab
uses document and scanned-table evidence for the template qualification rules.

Hosted channels draw from the vendor's serving capacity. Licensed and open
channels still need support staff and compatible hardware, supplied by their
customers. Each region and customer group shares one budget, one task pool
and one local compute pool across vendors. Monthly installed shares bound
discretionary migration in either direction. Feasible incumbents reserve work
first and remaining work is allocated by utility. The same money, compute and
product limits apply to both passes. Price, compatibility and capacity can
force departure. A delivery belongs to its product vendor.

### Controlled enrollment

Office Services can publish a waitlist on a live service. A zero limit pauses
new discretionary admissions. Signed work keeps priority. The busiest of the
last seven settled discretionary days represents existing demand, with ties
using the most recent day. Feasible established orders use the incumbent pass
of the allocator. New slots use spare serving capacity and the chosen daily
cap. They go first to eligible groups with the oldest continuous wait, then
to other eligible groups in proportion to remaining demand.

Waiting interest comes from a shared first-choice estimate within each buyer's
remaining money, hardware and demand. The queue groups daily task interest by
customer segment and never commits purchases or earns revenue. Other providers
can serve the same buyers through the ordinary shared allocation. The service
view shows the settled groups, dates and admissions. Reopening enrollment or
publishing a retirement notice ends the waitlist while retaining its history.

## Wholesale reservations

**Operations → Wholesale** is available in the planned-roster presets.
Commission a compatible owned rack, choose a plan and an operating buyer,
then review the term and confirm. The buyer protects the entire maximum fee
and metered electricity commitment from existing cash, with its operating
runway also accounted for. The seller earns a reservation fee after each
settled day and the buyer can use its imported slice for Lab work.

The catalog has two 50 CU/day plans with different terms, electricity
treatment and availability guarantees. Capacity is exclusive for the
term. Under curtailment, older agreements receive their promised slice
first. The daily fee falls in proportion to available capacity below
the guarantee. The fee is fully credited during an outage. The metered
plan reimburses incremental electricity from actual work, using the rack's
power curve, facility PUE and site tariff. The site meter counts each unit
of work once. The day posts equal buyer expense and seller revenue in
cash. Term expiry or closure releases unused funds and future capacity.
An owned rack cannot retire during an active agreement.

The save records capacity, actual work, credits, electricity and matching
ledger entries for every contract day. Restore checks both owners, the
dated hardware and escrow, work conservation and contract limits. Site
revenue is separate from model deliveries and installed share.

## Mac checks

```sh
ctest --test-dir build/release -R '^(core.allocation|runtime.regional-markets|runtime.competition|runtime.infrastructure)$' --output-on-failure
build/release/bin/pgp-game --smoke-test --infrastructure-smoke --text-scale 200 \
  --portable-root "$PWD/.work/regional-check"
```

Use a fresh scratch root for the explicit game check. The provider fixture
commissions two reference racks, ends the opening lease and fills four
funded blocks. It measures the operating profit after commissioning over
47 simulated days, including a full outage and restored buyer work. The
standard-roster Metal smoke finishes the three-year campaign and reloads
its 19,677,487-byte checkpoint under the existing 25 MiB limit.
