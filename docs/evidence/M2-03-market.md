# M2-03 contracts and demand — local Mac implementation

2026-11-04. Optimized Apple Clang arm64 build in the existing owned
`build/core-release` tree, with focused allocation/content/contract/resource
checks. No native matrix, Debug duplication, full regression, package or device
qualification was run.

## Implemented behavior

The [market module](../../src/core/MARKET.md) connects contracts, actual serving,
customer budgets and accounts. The [selected allocation procedure](M2-03-allocation-design.md)
now executes with canonical ID ordering, rational budget scaling before division,
shared proportional capacity, explicit outside demand and bounded retries.
Free work still consumes tasks and compute. A round must finish demand or retire
an exhausted product/edge; the kernel refuses failure to make proved progress.

Contract work receives shared-source priority before discretionary serving, with
finite training using the remaining compute. The final resource close receives
actual delivered demand, preventing unused serving reservations from creating
work or revenue. Fixed contract fees accrue by actual calendar month; usage bills
only delivered tasks. Service credits net off the fee once, while missing-task,
missed-start and cancellation penalties create separate supplier payables.
Contract fees and usage reduce the same customer budget before discretionary work.

Month-end installed shares derive from actual contract/discretionary delivery
and outside demand. Switching costs depend on other products' installed shares;
authored migration limits bound discretionary adoption. Commands, immutable
reports, recovery policy and exact saves preserve these consequences together.
Reload reconciles invoice amounts, protected/total serving work, contract-day
coverage and reconstructed monthly adoption.

The shared typed content loader/compiler now accepts optional scenario market
setup: serving profiles, prices, utilities, eligibility and contract terms.
Canonical content identity includes the setup, and malformed data or incompatible
typed state fails before scenario creation. The engineering fixture is
[market.json](../../tests/content/fixtures/market.json).

## Focused verification

| Check | Observed result |
| --- | --- |
| `./make.sh --core-only --jobs 4` | Optimized Mac build passes with warnings treated as errors; no application launch |
| `core.allocation` | 1,677 checks pass: unequal prices, 100/150 rational budget case, free work/retry, shared contention, edge limits, outside reasons, permutation invariance and bounded large counts |
| `content.market-setup` | 56 checks pass: authored setup/hash/save roundtrip, data ordering, invalid references/units/budgets/capacity/dates, incompatible typed state and atomic setup refusal |
| `runtime.market-integration` | 68 checks pass: exact resumed billing/work/shares, single customer budget, actual demand, protected outage service, credits/penalties, deadlines/cancellation, incumbency and malformed/legacy saves |
| `headless.market-demo` | Passes: 59,000 delivered tasks, zero outside tasks, 5,000,000 cents customer charges and 101,200,000 cents cash; resumed state matches exactly |
| Affected resource/application boundaries | `core.capacity`, `runtime.capacity-integration`, `headless.capacity-demo` and `session.persistence-policy` pass |

Eight distinct focused CTests pass. A final immutable-eligibility reload check
was added after review and the affected targets were rebuilt incrementally;
the market/content checks also pass after that correction.

Reproduce the authored example without a graphical window:

```sh
build/core-release/bin/pgp-headless --market-demo tests/content/fixtures/market.json
```

The example saves before January settlement and resumes through March 1. Two
months of $25,000 fees, $108,000 baseline costs and $5,000 non-compute support,
plus one $12,000 license, leave **$1,012,000**. Leased compute and included power
are not charged again. The example uses in-memory saves and writes no player file.

## Compatibility and remaining work

New saves pair `market_mode` with explicit market state; new business saves
refuse a missing/null market. Earlier foundation/accounts/capacity saves lacking
both fields keep explicit market absence, without fabricated customers or revenue.
Earlier capacity records lacking `protected_tasks` read as zero; market history
still must reconcile actual contractual priority. Older binaries cannot read the
new fields. Planning component/format identities remain unchanged; no release
was finalized.

The fixture starts with an engineering product, not a qualified M3 model. Issued
receivables currently collect on their due date. This owner has one company's
accounts; wider rival/region/channel economics remain later work. Existing
preferences are immutable in the primitive API. Finite history bounds fail
explicitly; century/sandbox retention and performance remain M8 work.

Formal accepted count remains **5/52**, retaining M2-02's open prerequisite.
M2-03's local implementation is ready for M2-04's three-year solvent strategy,
30-day workout, rescue and closure paths. Those paths are not claimed here.
