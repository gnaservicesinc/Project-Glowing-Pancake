# Slice management, delegation and scoring

`ManagementState` belongs to the simulation checkpoint. Its data-defined
catalog supplies fixed financing, one hireable specialist, a warned capacity
setback and the slice scoring rubric. Core commands own all consequences;
the Qt adapters format reports and queue the same commands.

## Finite standing policies

`AddDelegationPolicy` authorizes independent evaluation of one checkpoint or
renewal of one accepted, renewable compute lease. A campaign permits at most
eight policies, twelve accepted runs per policy and 128 decision records.
Evaluation specifies a plan, source, staff/rate ceilings, minimum interval,
total cash budget and reserve floor. Training can finish before the first
test; a live checkpoint is never evaluated. The previous test must stop
before another begins. Starting tests rechecks methodology prerequisites,
actual unreserved resources and the full fee. Neither scheduling nor the
preview samples a future evaluation draw.

Renewal is limited to the actual expiry morning. It pays setup again and
creates a separate recurring obligation for the next finite term. The policy
checks a monthly ceiling and debits setup plus `ceil(term_days / 28)` monthly
bills from its commitment budget, a conservative bound on the entire term.
The ledger still pays only actual setup and accrued service. The cash reserve
must cover that conservative new commitment. Accepting a lease remains
once-only; duplicate commands cannot clone its capacity.

Policies run in stable authorization order at the prepared morning after
required content decisions. Only actual eligible attempts copy a candidate;
idle daily reviews do not replay ledger or capacity history. A refused attempt
spends nothing, records its cause and suspends the policy. Suspending/resuming
keeps spent budget, accepted-run count and the next review date. There is no
automatic deployment, story response, financing, infinite retry or charter change.
Strict save loading reconciles decisions against real evaluation projects,
lease terms, payments, counters and cursors.

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
