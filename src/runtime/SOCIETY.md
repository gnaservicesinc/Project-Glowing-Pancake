# Regional society, standards and media

M6-02 adds the **World** room and one shared, saved regional society. The
eight profiles are fictional game balance, with authored population/output
weights and initial components. They are not official statistics.

## Player controls

Select World to compare population-weighted global welfare, the lowest region
for each index, and all eight individual regional profiles. The regional detail
shows the fourteen components and the latest actual delivery, public-service,
science, industry, promise, compute and supplier-concentration observations.
Advance to the next regional or forum date stops on its prepared morning.

Select a region, a deployed service and an audience to review a media campaign.
The review names the fee, independent report, awareness gain and exclusive end.
Payment requires a separate confirmation and unreserved cash. Repeats to the
same region/service/audience in the same month yield `base / (repeats + 1)`
awareness, with at most three campaigns per month in the bundled content.
Customer awareness raises the existing allocator's utility by at most 30%.
Price willingness, migration, customer hardware, supply and eligibility still
apply. Other audiences have separately visible awareness. Spending does not
change model capability, evidence, reputation or welfare directly.

During a standards hearing, select an undisclosed member region and a completed
independent report. Review support/opposition and its fee before confirming.
One disclosure changes that member's vote score. Five of eight supporting
members are required in the bundled proposal, and only supporting members
implement it, after their individual delays. Declining members keep their own
access rules. Each round expires, and a later hearing needs another ballot.

The bundled standard governs agents: an independent documents lower bound of
65/100 is required after the destination member's effective date. It applies
to live delivery as well as new deployments. Complete the required evaluation
and redeploy with that report to restore access. Human review does not waive
this standard. Signed business promises retain their credits and penalties
when an authored standard blocks work. Agent sales retain their existing
discretionary-market contract; this feature does not create agent subscriptions.

## Observations and welfare

After every seller settles a calendar month, the runtime visits that month's
actual discretionary and contract receipts. Customer demand is counted once
across companies. Source records determine delivered tasks, sector access,
missed promises, serving compute and independently measured deployed quality.
Concentration combines subsidiaries acquired by the month end with their
controlling company. The initial partial month, if any, starts at the actual
campaign opening. No future report or delivery is used.

The bundled lag is two months: January's observations first affect components
at the March close. Each component moves from its current value toward its
initial value plus a bounded outcome signal, at the region's authored monthly
adaptation rate. The change rounds half up. Components are bounded to 0–100.
Indices use equal component weights, with integer truncation at 0.01 point:

| Index | Equally weighted components |
| --- | --- |
| Basic needs | Food, housing, essential-service access |
| Health | Care access, prevention |
| Civic agency | Institutional checks, participation, information integrity |
| Ecological resilience | Emissions trend, resource resilience, adaptation capacity |
| Shared prosperity | Income opportunity, transition support, distribution |

The outcome signals use basis points. `U` is useful delivered work / total
demand; `P` is public-service work / public-service demand; `S`, `I` and `A`
are science, industry and agent work / total demand; `M` is missed promises /
total demand. Zero denominators produce zero. `H` is supplier HHI weighted by
`U`. `Q` is the delivered independent lower bound minus 50/100, weighted by `U`.
`E` is capped serving-compute intensity, also weighted by `U`. This prevents a
token delivery from producing a region-wide quality, concentration or energy
effect. `T = P/3 + S/2 - A/3` represents transition support versus displacement.

| Component | Target change from its initial value |
| --- | --- |
| Food | `U/5 + P/8` |
| Housing | `I/5 + P/10` |
| Essential services | `P/3 - M/4` |
| Care | `P/4 + Q/8` |
| Prevention | `S/3 + P/8` |
| Institutional checks | `P/10 - H/10` |
| Participation | `P/6 - A/12` |
| Information integrity | `Q/4 - M/5` |
| Emissions trend | `U/8 - E/5` |
| Resource resilience | `P/8 - E/8` |
| Adaptation capacity | `S/3 + P/8` |
| Income opportunity | `I/3 + U/8 - A/6` |
| Transition support | `T` |
| Distribution | `U/6 - H/8 + T/3` |

Integer divisions truncate toward zero. Serving intensity is
`min(10000, floor(serving milli-CU / delivered tasks) * 5)` before uptake
weighting. It is a regional service-footprint proxy; actual owned-site energy,
water and carbon ledgers remain in Operations. These early-era coefficients
are explicit abstractions. Century progression, objective feasibility and
long-run balance remain M7/M8 work.

Six separate reputation scores start at 50/100: investors, researchers,
customers, staff, regulators and communities. They follow the player's actual
delivered independent evidence and promise reliability. Research/science and
public-service work affect the relevant audiences. Paid awareness is separate.
Neither reputation nor advertising grants institutional authority.

## Shared runtime and persistence

`regional_society` is a typed content definition referenced by each scenario.
Studio exposes its regions, components, sector references, lags, media and
forum terms through the shared forms and reviewed reference changes. The
fourteen-component shape and eight-region shape are fixed. Sectors must refer
to distinct actual customer groups covering every region.

The shared world owns society; individual rival checkpoints do not duplicate
it. Opportunity calculation, wholesale preview and actual settlement receive
the same derived eligibility and awareness context. Clients cannot inject that
context through an AdvanceDay command. Ordinary commands still use one copied
world transaction, stale revisions, protected cash and commitment autosaves.
World facts and briefings use the existing typed flow runtime.

Save/restore reconciles every monthly input against source deliveries, then
recomputes component lags, audience updates, forum scores and member dates.
Campaigns retain their original deployment index and exact expense posting;
same-day redeployment and later service withdrawal preserve that disclosure.
Unknown fields, forged outcomes, absent payments and unauthorized society state
are refused. Older content identities need an explicit migration or remain
preserved under their original content; no automatic cross-content rewrite is
provided. Monthly observations are bounded to 6,144, campaigns to 4,096 and
forum rounds to 256. Daily work does not replay historical months.

Focused engineering checks are `runtime.society`, `studio.world-forms` and
`bridge.management`. The explicit `pgp-game --smoke-test --society-smoke`
walkthrough uses an isolated `--portable-root`, real QML review/confirmation
controls, a calendar stop and exact save reload. Human sessions and native
release qualification remain deferred.
