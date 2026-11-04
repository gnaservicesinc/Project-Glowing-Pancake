# M2-02 shared capacity — local Mac implementation

2026-11-04. One incremental optimized Apple Clang arm64 build in the existing
`build/core-release` tree, followed by seven focused CTests. No Debug, native
matrix, full regression, package or device qualification was run.

## Implemented behavior

The [capacity module](../../src/core/CAPACITY.md) owns physical source identity,
leased/owned provenance, nominal/effective milli-CU/day, staff availability,
memory/interconnect compatibility and finite work or ongoing serving projects.
New commitments fit effective supply and available staff. One physical slice
cannot be represented twice or resold through this API.

Daily close uses the common source snapshot, allocates serving first, then
proportionally allocates remaining supply to finite work with stable ID residue
ties. Inference uses checked positive milli-CU per useful task and whole tasks;
neither outages nor cancellation create useful-task credit or free capacity.
Finite work completes once and releases future assignments. Its model/research
consequence is for later owning systems on the next morning. Cancellation keeps
past completed work, consumed licenses and accrued charges.

The simulation closes capacity before accounts, then advances/prepares the next
morning. It validates shared owner/start/close cursors. Fixed serving support
costs own separate accounts obligations; cancellation ends future support while
retaining past accruals. Immediate unused cancellation has no service charge.
Capacity creates no second charge for the existing lease or its included power.
Commands share revision checks, candidate rollback, receipts, recovery saves and
immutable reports. Exact saves retain sources, commitments, daily source usage,
work/tasks/staff and cancellation/completion cursors.

## Focused verification

| Check | Observed result |
| --- | --- |
| `./make.sh --core-only --jobs 4` | Optimized Mac build passes; warnings treated as errors; no application launch |
| `core.capacity` | 88 checks pass: simultaneous 20/60 CU work, staff/compute refusal, compatibility, duplicate physical source rejection, whole tasks, outage priority/fair residues, cancellation, completion and malformed history |
| `runtime.capacity-integration` | 52 checks pass: 60-day work, separate support costs, exact resumed state, outage/recovery, stale/invalid commands, immediate/used support cancellation and strict/legacy saves |
| `headless.capacity-demo` | Passes; 3,600 CU training complete, 60,000 engineering serving tasks and 96,200,000 cents cash with exact resumed state |
| Accounting/application boundaries | `core.accounts` (80 checks), `runtime.accounts-integration` (59), `headless.accounts-demo` and `session.persistence-policy` pass |

Reproduce without a graphical window:

```sh
build/core-release/bin/pgp-headless --capacity-demo tests/content/fixtures/foundation.json
```

The example isolates the authored opening balance and one 100 CU/day lease with
six staff. Serving reserves 20 CU/day and one person; training reserves 60 CU/day
and four people for 3,600 CU. Serving adds $5,000/month of non-compute support.
The data license costs $12,000 once. After 60 days, two settled months of baseline
and support leave **$962,000**. The March partial month remains accrued, rather
than being charged as a full third month. A Jan31 checkpoint resumes identically.
No actual customer contract/revenue or qualified model is claimed by this
engineering workload.

## Compatibility and remaining work

New saves pair `capacity_mode` with explicit capacity state. New business mode
refuses missing/null state. Original foundation/accounts-only saves lacking both
fields retain explicit capacity absence; no sources or work history are
fabricated. Older binaries cannot read the new fields. Development component
identities remain planning values and no release was finalized.

The opening generic lease has memory zero and `general` interconnect until
hardware/recipe catalogs define richer requirements. Positive memory requests
need explicit compatible sources. Owned-site power, wholesale buyer/seller
contracts and global capacity ownership remain M5 work. Demand allocation,
revenue/refunds and installed customer shares are M2-03; research/model effects
are M3. Finite record limits and later century/500-year compaction/performance
remain explicit in the module contract.

M2-02's local implementation is complete for the shared resource primitives;
formal backlog acceptance retains M2-01's open prerequisite. Accepted count stays
5/52. Next: freeze M2-03's finite allocation procedure, then implement contracts.
