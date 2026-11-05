# Liquidity warnings and recovery

`liquidity.hpp` is the M2-04 owning module for business operating status. It
observes committed accounts after daily accrual/collection/payment, then exposes
the next morning's warning. It never changes journal balances, writes off a
liability, forgives principal or creates rescue funds.

A warning begins only when a payable whose due date has completed remains
unpaid, or a matured loan still has principal outstanding. The exposure keeps
payable and matured-principal amounts separate and names the oldest obligation
by `(due_date, kind, stable_reference)`, retaining its cause. Negative accounting
profit, zero cash and accrued costs that are not due do not independently cause
insolvency. Funding alone also does not service an existing liability: ordinary
invoice-settlement or principal-repayment transactions must cure it.

If the first failed settlement closes date D, the warning is presented on morning
W = D+1. Its saved deadline is W+30 calendar days. The coordinator pauses and
requires one acknowledgment before advancing work; financing and settlement
commands remain available while the warning is unacknowledged. Acknowledgment
does not erase obligations or restart the deadline. Partial funding/payment
keeps the same workout and deadline.

Rescue commands remain available on the deadline morning. If obligations are
cured by those commands, refresh resolves the workout immediately. If they
remain unpaid when the player advances on that morning, the coordinator closes
the workout **before** any additional capacity, market or accounting activity.
The date does not advance: exactly 30 elapsed calendar days have been available
since warning W, with no extra deadline-day service interval. Closure preserves
accounts, resource history and the causal workout record for a postmortem. This
module cannot restart a closed company.

Every warning has a monotonic identity, immutable initial shortfall/cause,
warning/deadline dates, acknowledgment and either rescue or closure date. A fully
resolved company may later experience a genuinely new unpaid obligation and get
a new window; unresolved partial rescues cannot manufacture extra time. History
is bounded to 4,096 workouts and is never silently dropped.

The observation cursor follows the last closed accounts day exactly. Command
refresh reads only obligations due through that completed day, so an invoice
issued today is not called unpaid before today's settlement. Loading checks
current exposure against owning accounts and verifies retained causal references
and workout timing. It performs no historical calendar replay or automatic work.
Mutations validate a copied candidate and commit atomically; stale/duplicate
acknowledgments and invalid cursor changes leave the prior state intact.

The initial recovery mechanisms are existing validated debt/equity financing,
reservation cancellation and settlement/repayment commands. Asset-sale,
negotiated restructuring and merger mechanics remain with their later owning
systems. This state machine does not implement the six century objectives or
claim that a dormant cash shell satisfies the Survival objective.
