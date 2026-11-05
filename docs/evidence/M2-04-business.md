# M2-04 business scenario and liquidity recovery

2026-11-04. Local optimized Apple Clang arm64 implementation using the existing
owned `build/core-release` tree. No cross-platform CI, Debug duplication, package,
full release regression or clean-device qualification was run.

## Implemented behavior

The [liquidity owner](../../src/core/LIQUIDITY.md) observes actual due payables and
matured principal after daily settlement. It preserves the oldest causal
obligation, initial shortfall, warning/deadline, acknowledgment and rescue/closure
history. Negative accounting profit, zero cash or costs not yet due do not cause
insolvency by themselves.

A failed settlement on D warns on morning W=D+1 and pauses for acknowledgment.
Recovery commands can service liabilities on the deadline morning W+30.
Advancing without a cure closes the company at that boundary before further
work, billing or date advancement. Partial payments and reloads keep the original window. Financing alone
does not erase obligations. Terminal closure rejects further work/flow mutations,
keeps accounts and history inspectable, and triggers the campaign-end autosave.

The [scenario runner](../../src/session/SCENARIO_RUN.md) uses typed, hashed
`business_plan` data for the startup license/workload, expansion date/terms and
one equity rescue. It executes ordinary revision-checked commands, stops for
required choices, and reconstructs policy progress from committed identities
after reload. Monthly reports derive income/costs/balances from postings and
useful work from actual market receipts. The fixture is
[business.json](../../tests/content/fixtures/business.json).

The expansion policy preserves the opening $1.2 million, six staff, 100 CU/day
lease and $108,000 monthly baseline costs. Initial serving reserves 20 CU/day
and one person; training uses 60 CU/day and four people for 60 days. After that
work completes, a second enterprise customer cohort reserves 80 CU/day and one
person on the same lease. Its authored $100,000 monthly fee and $5,000 support
combine with the initial $25,000 fee/$5,000 support. Compute and power are not
charged again. No external financing is used by this policy.

## Focused verification

| Check | Observed result |
| --- | --- |
| Optimized Mac build | Passes with warnings treated as errors; existing build tree reused, no app launch |
| `session.business-scenario` | Passes in 174.34 seconds: uninterrupted three-year policy versus an exact July 1, 2015 save/resume, all 36 monthly reports and final canonical save bytes |
| `session.business-recovery` | Passes in 8.21 seconds after exact-deadline refinement: baseline warning, closure, preserved liabilities and equity rescue with actual overdue payments |
| `core.liquidity` and `runtime.liquidity-integration` | Pass: acknowledgment, fixed deadlines, partial cure, matured debt, deadline-morning rescue, closure before further work and strict/corrupt/legacy saves |
| Affected boundaries | `content.market-setup`, `runtime.market-integration` and `session.persistence-policy` pass, including warning halt and exact closure autosave |
| Actual CLI baseline run | Warns 2015-03-01, closes 2015-03-31 before consuming that day's work, retains 12,916,127 cents total payables including 1,980,646 cents overdue; exact in-memory March 1 reload |

Seven distinct focused CTests pass across this batch. After the final deadline
refinement, the small liquidity, recovery and application-policy checks were
rerun; the healthy three-year path is unchanged. No full-suite regression ran.

The healthy expansion completes 2017-01-01 with **124,693,549 cents cash**,
429,677,419 cents revenue, 424,983,870 cents expenses, no debt or unpaid bills,
and **5,240,000 delivered tasks**. An independent arithmetic check agrees with
the frozen ledger totals. Monthly reports sum to authoritative income/cost/work
history, and the saved midpoint continuation equals uninterrupted execution.

The first three-year comparison reached its initial 180-second check timeout.
A one-second local sample identified repeated account validation. Liquidity
operations now validate each public input ledger once, retaining all exposure,
history and link checks. Market history uses validated contiguous capacity-day
indices and month ranges instead of repeated full-history searches. The focused
comparison has a bounded 300-second timeout; no check or history was discarded.
Review also corrected recovery assertions that confused future month-end
accruals with overdue bills. The final cutoff is exactly 30 elapsed days: rescue
commands remain available that morning, then an unresolved advance closes
without new income, accrual, work or date movement. The CLI reports total and
overdue payables separately.

## Run and inspect

```sh
build/core-release/bin/pgp-headless --business-run tests/content/fixtures/business.json expansion
build/core-release/bin/pgp-headless --business-run tests/content/fixtures/business.json baseline
build/core-release/bin/pgp-headless --business-run tests/content/fixtures/business.json rescue
```

The CLI prints monthly reports and causal workout dates, and explicitly exercises
an in-memory March 1 reload. It writes no player files or launches a window.
`baseline` acknowledges its warning and demonstrates closure; `rescue` records
one equity investment plus actual overdue payments, then stops at a subsequent
warning. Normal application drivers halt at the required initial warning.

## Compatibility and remaining work

New saves pair `liquidity_mode` with explicit liquidity state. New business mode
refuses missing/null state; earlier saves lacking both fields retain explicit
absence rather than fabricated workout history. The new `closed` phase is
separate from reaching the normal campaign end. Older binaries cannot read new
fields. Development version/format identities remain planning values and no
release content batch was finalized.

The scenario starts with an engineering product; it does not satisfy M3's
data/research/model/evaluation requirements. Receivables currently collect on
their agreed due date. Asset sales, negotiated restructuring and mergers belong
to later owning systems. Three-year policy evidence does not qualify century
performance, balance, player usability or the six final objectives. History and
save bounds remain explicit and required records are never silently dropped.

Formal accepted tasks remain 5/52 while prerequisite gates stay open. The next
Mac presentation task is M1-05's accessible Office/Lab shell; M1-06 related scopes
and M1-07 built-in event dispatch also remain foundation work before the complete
first playable and Content Studio.
