# M2-01 accounts — local Mac implementation

2026-11-04. Optimized Apple Clang arm64 build using the existing owned
`build/core-release` tree. No native matrix, Debug duplication, package or device
qualification was run for this batch.

## Implemented behavior

The [accounts module](../../src/core/ACCOUNTS.md) owns balanced causal postings,
invoice registers, fixed recurring terms, debt and fully diluted ownership.
Scenario creation uses authored cash, commitments and ownership. Every business
checkpoint reconciles simulation cash with posting-derived cash and preserves
the exact last-closed-day cursor. Immutable snapshots expose derived reports.

Reservations recognize no expense until consumption. Recurring obligations
accrue using actual days in each calendar month, then settle when due. Due
receivables collect before payments; each group uses `(due,id)` order. Payments
preserve reserved cash and leave unfunded amounts payable. An invoice's income
or expense is recognized separately from collection/payment. Monthly debt
interest and explicit principal repayment remain distinct from product revenue,
as do equity proceeds. Equity dilution conserves 10,000 basis points with stable
largest-remainder rounding and one reserved pool.

Commands share revision checks, candidate rollback, receipts and the existing
recovery/autosave boundary. Projections close a copy without changing RNG or live
balances. Strict saves include each posting/document link, obligations, partial
settlements, debt interest residues and cap-table state. Validation reconciles
individual documents and reconstructs fixed accrual/interest history. Reserved
transaction namespaces prevent caller keys from preempting future settlements.

## Focused verification

| Check | Observed result |
| --- | --- |
| `./make.sh --core-only --jobs 4` | Optimized Mac build passes with warnings treated as errors; no application launch |
| `core.accounts` | 80 checks pass: 28/29/30/31-day months, partial terms, $972,000 opening result, pure projections, protected/partial payments, invoice identities, debt/interest, cap conservation/rounding and atomic refusal/overflow |
| `runtime.accounts-integration` | 59 checks pass: authored scenario, commands, Jan31 pre-settlement save/resume, each-day parity, financing, protected cash recovery, stale commands and invalid saves |
| `headless.accounts-demo` | Passes; two months plus $12,000 license leave 97,200,000 cents, 22,800,000 cents expenses, zero revenue/payables and exact resumed state |
| Existing affected boundaries | `core.simulation`, `runtime.save-integration`, `session.persistence-policy`, `persistence.local-save-settings` and `headless.flow-demo` pass |

Eight distinct CTests pass in the focused selections. After the initial build,
the accounts test target alone was incrementally rebuilt for added namespace
and corrupt-document checks. No full-suite or cross-platform result is claimed.

Reproduce the opening example:

```sh
build/core-release/bin/pgp-headless --accounts-demo tests/content/fixtures/foundation.json
```

The demonstration disables unrelated engineering story roots, consumes the
license once, saves/reloads before January settlement, and runs through March 1.
It uses in-memory serialization and writes no player save file.

## Compatibility and remaining work

New saves pair `accounting_mode` with explicit accounts. Business mode refuses a
missing/null ledger; all fields and authoritative integer tokens are strict.
Original engineering saves lacking both fields load as foundation checkpoints
without fabricated financial history. Removing both fields and recomputing an
integrity digest is indistinguishable from original legacy data; the digest is
not a hostile-tampering guarantee. Older binaries cannot read the new fields.
Planning component/format identities remain unchanged; no release was finalized.

Issued receivables currently collect on their agreed due date. Customer
uncertainty belongs to contracts. Debt maturity appears separately in
projections and requires an explicit repayment command; liquidity warnings,
30-day workouts and rescue/closure remain M2-04. Board/vesting/negotiation and
mergers remain later tasks. Finite history limits fail explicitly; century
retention/compaction and performance remain M8 work.

M2-01's local implementation is complete for its accounting primitives; formal
backlog acceptance still retains M1-03's open prerequisite. Accepted count stays
5/52. Next Mac implementation: M2-02 shared capacity and project commitments.
