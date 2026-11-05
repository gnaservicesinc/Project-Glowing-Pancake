# Site hazards and resilience

Use **Operations → Resilience** to inspect the selected state's current
monthly exposure, install site protections, insure an owned site or reserve
a geographic service replica. Each purchase has a confirmation showing its
actual fee, lead time and obligations. A site incident pauses the campaign
for review. Its news, repair bill, recovery date and insurance settlement
remain in the local save.

The shared resilience catalogue supplies fictional monthly profiles for all
50 states and DC. Each commissioned site with live hardware is sampled once
at the start of a month. The six outcomes cover grid outages, heat, storms,
floods, fire and earthquakes. Every profile retains a quiet outcome.
Probabilities are authored game balance values.

The sampler uses the saved hazards stream. Site samples and content incident
draws share its ordered history. A selected occurrence has a saved onset
within the month. Player projections show published exposure and actual
incidents. Previewing a site or changing render speed consumes no draws.

## Physical recovery and protection

An incident first changes the rack's actual effective capacity. Its repair
invoice recognizes the loss and remains a company obligation. Repair time
and payment must both complete before the physical repair is finished.
Some hazards then require a site data restoration interval. These limits
feed the same capacity allocator, meter and customer settlement as ordinary
owned work. Wholesale buyers receive their contracted availability credits.

The five protections have separate effects.

- A generator supplies a bounded amount of facility power during a grid
  outage, for its stated fuel endurance. Actual work drives metered fuel cost
  and emissions.
- Tested offsite copies shorten the site data restoration interval.
- Preventive inspections reduce the fire probability at monthly sampling.
- Physical preparedness reduces repair time and cost for fire, storm,
  flood and earthquake outcomes.
- Supplemental heat management reduces the thermal capacity loss.

Installation lead times apply before protection takes effect. Setup fees
are spent, and monthly upkeep starts on the installation date. Closing a
site ends future upkeep while accrued obligations remain payable.

## Replication

A qualified hosted service can reserve a compatible owned rack in a
different state. The spare must have room for the service's full serving
reservation plus its synchronization workload. Synchronization uses real
compute, staff and monthly support. Initial copy work must finish before
the replica is ready.

When primary capacity falls below the service reservation, a ready replica
can move that same measured deployment to its funded spare. The market
keeps the existing product, contracts and useful-work history. The primary
reservation becomes standby capacity. When the primary can carry the
service again, it resumes serving and the spare returns to standby.

Stop the replica before replacing its model, cancelling its workloads or
retiring either rack. Stopping releases the standby reservation and ends
future synchronization costs. Ordinary management remains available while
an outage is being repaired.

## Insurance

A policy names covered hazards, a waiting period, deductible, reimbursement
fraction, aggregate limit and claim delay. The insurer protects its whole
maximum liability from a finite cash pool. A later policy cannot cover
earlier damage.

Covered incidents commit a claim against the policy's remaining limit.
The due payment reduces insurer cash and credits the company's repair
expense by the same amount. Physical recovery continues to follow the paid
repair and data restoration stages. Expired coverage releases unused
funds while approved unpaid claims retain their protection.

## Persistence and checks

Daily preparation processes active incidents and new monthly samples.
Full restore validation replays seasonal draws, effective capacity, metered
energy, qualified replica transfers, reservation ownership, repair invoices,
insurance limits and causal notices. Histories retain the exact ordered
model/evidence boundaries of each replica.

```sh
ctest --test-dir build/release -R '^(core.capacity|runtime.resilience|runtime.infrastructure|runtime.regional-markets)$' --output-on-failure
build/release/bin/pgp-game --smoke-test --infrastructure-smoke --text-scale 200 \
  --portable-root "$PWD/.work/resilience-check"
```

Use a fresh scratch root for the explicit app check. It confirms paid
generator and insurance terms through the keyboard, then completes the
owned-site, wholesale and exact-reload path. Matched integration fixtures
cover physical and financial recovery, paid replication, finite emergency
power, heat protection, rejected spending and forged saves.
