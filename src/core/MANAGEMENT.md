# Slice management, delegation and scoring

`ManagementState` belongs to the simulation checkpoint. Its data-defined
catalog supplies fixed financing, one hireable specialist, a warned capacity
setback and the slice scoring rubric. Core commands own all consequences;
the Qt adapters format reports and queue the same commands.

## Finite standing policies

`AddDelegationPolicy` authorizes independent evaluation, paid compute lease
renewal, bounded pricing, an operating cash reserve or replacement of one
owned rack. A campaign retains up to 256 authorizations with eight enabled
at once. The Office pages through up to 65,536 decisions.
Evaluation specifies a plan, source, staff/rate ceilings, minimum interval,
total cash budget and reserve floor, with up to twelve accepted runs.
Training can finish before the first test. Use an immutable checkpoint to
evaluate a running model. A previous test must stop before another begins.
Starting tests rechecks methodology prerequisites, unreserved resources and
the full fee. Evaluation draws occur when the work settles.

Renewal is limited to the actual expiry morning. It pays setup again and
creates a separate recurring obligation for the next finite term. The policy
checks a monthly ceiling and debits setup plus `ceil(term_days / 28)` monthly
bills from its commitment budget, a conservative bound on the entire term.
The ledger still pays only actual setup and accrued service. The cash reserve
must cover that conservative new commitment. Accepting a lease remains
once-only; duplicate commands cannot clone its capacity.

Bounded pricing reviews recent settled discretionary demand against available
serving slots and moves a public quote by at most one authorized step. Each
review has a saved before/after price event. Manual price changes suspend the
policy while preserving its audit. Existing contracts retain their terms.

One operating reserve can protect a chosen cash target at a time. It reduces
cash available for new discretionary commitments, including ordinary and
frontier delegation. Paid delegated actions use the higher of their own floor
and the operating reserve. Accepted bills, principal repayments and story consequences can draw
down the reserve. New receipts refill it before becoming available for new
spending. Authorization, suspension and resumption appear in the policy log.

A hardware replacement authorizes one specific installed rack, one compatible
family and separate purchase and monthly maintenance ceilings. It can order
at the original rack's rated useful-life date, measured from delivery. The
site needs a spare slot and enough power and paid operators to stage both.
After the successor commissions and has at least the old rack's rated usable
capacity, the policy waits for the original rack's active and paused work,
customer, wholesale and replica commitments to clear. Retirement uses the
ordinary disposal ledger. A suspended in-flight replacement retains its paid
order, and resumption uses that same order.

Policies run in stable authorization order at the prepared morning after
required content decisions. Only actual eligible attempts copy a candidate;
idle daily reviews do not replay ledger or capacity history. A refused attempt
spends nothing, records its cause and suspends the policy. Suspending/resuming
keeps spent budget, accepted-run count and the next review date. There is no
automatic deployment, story response, financing, infinite retry or charter change.
Strict save loading reconciles decisions against real evaluation projects,
lease terms, quotes, reserve controls, hardware orders and retirements.

## Recovery and financing

The canonical seed announces supplier maintenance on 2014-04-17, reduces the
opening source to 60 CU/day on 2014-05-01, and restores 100 CU/day on 2014-05-15.
The start requires acknowledgment before further work; that acknowledgment
neither repairs capacity nor refunds consumed costs. Existing serving promises
retain priority, so optional training can finish later. The player can wait,
stop optional work, change scope or pay for another compatible source.
The notices retain actual warning, start and restoration dates across reload.

The specialist costs $2,000 to sign and $6,000/month, adding one available
person from the hire date. Historical daily staffing retains its earlier
limit. No skill/capability bonus or free compute is invented. The efficient
recipe and its research support a separate document-specialist service with
3,000 tasks/day and a 12-milli-CU inference cost; it still needs independent
model qualification.

Each financing offer can be used once. Bridge equity pays $300,000 for 18%
proportional dilution. Working capital pays $250,000 with 1% monthly interest
and principal due after 365 days. Actual ledger and ownership rules apply;
proceeds are not operating profit. Principal repayment is explicit, and a
paid-off loan does not generate a new maturity reminder.

## Clock and score

Local preferences separately control project completions, evaluation results
and company advance-warning alerts. Required choices, the capacity warning,
actual principal/lease/start deadlines and the final insolvency response day
still stop the driver. `session::Milestone` captures existing project IDs or a
known actual deadline; it never forecasts random results or invents news dates.
The worker advances one validated day per tick, remains responsive and caps
running snapshot publication at 10 Hz. Normal speed is one day/second.

The pure slice report scores survival to 2017 (25), actual delivered work up to
four million tasks (25), positive operating profit (20), independently qualified
deployed domains (20) and retained founder stake (10). Weights/targets are
content data. Human review does not turn a weak model domain into measured
capability. Scores are provisional until completion/closure, derived from
owned records after reload, and do not award the later century objectives.

Verification lives in `runtime.management` (catalog/save boundaries, budgets,
resource refusal, renewal, hiring/dilution/debt, warning/recovery, pause/deadline
routing and a scored specialist ending), `bridge.management` and the explicit
rendered game smoke. The `--specialist-demo` headless strategy uses ordinary
commands and finite policies, separately from the player's standing policies.
