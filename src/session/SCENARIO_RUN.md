# Three-year business scenario runner

`run_business_scenario` executes the compiled scenario's authored `business_plan`
through the same revision-checked commands as the game. It creates no cash,
compute, research result or customer agreement by editing live state directly.
The fixture starts with an engineering product; M3 still supplies real data,
training/model qualification and product launch rules.

The runner is bounded to the 2014–2017 slice (at most 1,096 service days). It
prepares each morning, respects required core/content decisions and stops at the
exclusive scenario ending or a closed-company state. The default driver stops
at an unacknowledged liquidity warning. An explicit engineering option may
acknowledge a warning and continue service through its disclosed workout window;
this is the intentional no-rescue policy used to demonstrate closure.
The warned morning plus 30 elapsed days is the deadline morning. Rescue commands
remain available there, but advancing unresolved closes the company before any
additional service is consumed; the deadline does not grant a 31st work day.

The three policy choices use data-defined amounts and dates:

- `baseline` buys the initial data license and completes the finite training
  workload while continuing the original contract. Its cash burn eventually
  creates overdue supplier bills and a warned workout.
- `expansion` follows the baseline until the authored expansion date, requires
  the original finite training goal to be complete, and then reserves the
  authored serving profile and signs/starts the added enterprise agreement.
  Initial and expanded service share the same source; capacity and staff checks
  can refuse the plan rather than inventing availability.
- `rescue` follows baseline until its first workout, invests the authored equity
  amount once, then explicitly settles already-due supplier bills and matured
  principal from available cash. Financing is equity, not product revenue. The
  driver stops at a later warning rather than inventing another funding round.

The business fixture preserves the opening $1.2 million, six staff and 100 CU/day
lease. Initial service uses 20 CU/day plus one person, and finite training uses
60 CU/day plus four people for 60 days. On March 2, after training completes,
the expansion reserves the remaining 80 CU/day and one person for 4,000 tasks.
The added $100,000/month contract has separate $5,000/month support. Together
with the initial $25,000/month fee and $5,000 support, revenue exceeds baseline
and support costs after expansion without external funding.

`stop_on` stops before policy/work on a chosen prepared morning. A resumed run
reconstructs policy progress from committed reservation/project/product/contract
IDs and financing keys. It needs no separate unsaved policy cursor. The optional
`reload_on` boundary performs exact in-memory serialization and restoration,
without touching a player file or advancing work. Content/schema identities are
checked by the existing save decoder. Required choices are never auto-selected.

Reports derive each month's revenue, expenses, cash, debt and payables from
chronological journal postings, and useful/outside tasks from market receipts.
The final partial month is reported through its last consumed day. Monthly
income/cost/work totals sum to the corresponding authoritative histories;
balances are reporting queries and never drive a state change.

The focused three-year check compares uninterrupted execution with a saved
midpoint continuation, and separately exercises baseline warning/closure and an
explicit equity rescue. This is business-simulation evidence, not a claim of
player usability, finalized balance, M3 model quality or century performance.
