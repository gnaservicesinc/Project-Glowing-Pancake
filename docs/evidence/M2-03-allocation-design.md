# M2-03 finite allocation design

2026-11-04. Selected kernel procedure for the next implementation batch, following
the [customer-allocation contract](../SIMULATION_SPEC.md#customer-allocation).
These are hand-worked design cases, not passing market-runtime evidence.
The subsequent [market implementation evidence](M2-03-market.md) records the
implemented procedure and its focused checks; this document preserves the
design selected before coding.

## Common-snapshot procedure

Inputs are validated remaining segment tasks/budgets, product prices and useful-
task capacities, eligibility/willingness-to-pay, integer utility weights and the
outside-option weight. Weights remain fixed throughout one daily allocation.
Stable IDs determine all remainder ties. Contract work/fees reserve tasks, cash
and compatible capacity first, exactly once; the kernel receives their residuals.

For each round:

1. Exclude exhausted products and edges whose per-task price exceeds that
   segment's remaining budget. Free products remain eligible. If no product is
   eligible, finish remaining tasks in the outside option with a recorded reason.
2. Hamilton-apportion each segment's remaining tasks across eligible products and
   the outside option: floor rational quotas, then largest remainders with stable
   IDs. Commit outside tasks immediately; never recontest them in later rounds.
   If every eligible weight is zero, finish the row outside with a zero-utility
   reason instead of dividing by zero.
3. For product claims `q_i`, compute checked tentative cost `C = sum(p_i*q_i)`.
   If `C > B`, replace claims by `floor(q_i*B/C)`, retaining exact remainders.
   Offer at most one additional task per positive remainder, in remainder/ID
   order, only when it fits the original claim and shared remaining row budget.
   Multiply before division; never truncate `B/C` to an integer first.
4. Resolve all segments' claims on each product together. When claims exceed its
   remaining task capacity, Hamilton-apportion that capacity proportionally to
   claims, breaking ties by segment ID. Otherwise fulfill all its claims.
5. Commit only delivered tasks and their costs. Decrement actual budgets/supply;
   keep refused quota as explicit residual demand. Retire exhausted products and
   newly unaffordable edges using **actual delivered spend**, then repeat.

Budget-refused tasks can retry free/cheaper products. Capacity-refused tasks can
retry remaining supply. Every task ends as delivered work or outside-option
work; free work still consumes demand and compute. This is a disclosed bounded
policy, not an optimal-utility solver.

## Termination and work bound

Let `P` be initial products and `E` initial eligible segment-product edges.
A round with any capacity refusal exhausts at least one product: integer
apportionment uses exactly its remaining task capacity. That product cannot
return during the same daily allocation.

If there is budget refusal without capacity refusal, at least one positive-price
edge becomes unaffordable or the row's money reaches zero. For integer claims,
`x_i=q_i*B/C` has exact weighted cost `sum(p_i*x_i)=B`. All floor/ceil proposals
remain within original claims. If every ceil fits, it spends the budget exactly;
otherwise a refused positive-price ceil costs more than the final residual
budget. Free ceils cannot be refused for money. Because capacity did not curtail
delivery in this case, that edge's retirement persists. With no refusal, all
remaining demand completes.

Thus at most `P + E + 1` rounds are needed. With checked catalog bounds and
remainder sorting, work is `O((P+E+1)*E*log(E+1))`, memory `O(E+S+P)` for `S`
segments. There is no per-task loop or arbitrary convergence-round limit.
Validation/overflow/work-bound failures must return a recoverable traced error,
preserving the pre-allocation state.

## Hand-worked cases

| Case | Inputs | Expected allocation |
| --- | --- | --- |
| Unequal prices and outside | 7 tasks; weights A:1/B:2/outside:1; prices A:3/B:7 cents; budget 30; adequate supply | Ideals 1.75/3.5/1.75; integer A:2/B:3/outside:2; cost 27 |
| Rational 100/150 budget | Initial claims A:5 at 10/B:5 at 20; budget 100 | Scaled ideals 10/3 each; floors 3/3 cost 90; equal remainders award A one; final A:4/B:3/outside:3, cost 100 |
| Free product retry | 10 tasks; equal free/paid weights; paid price 20; budget 40; free capacity 10 | First integer claims 5/5 scale to 2/2; paid edge retires; six residual tasks retry free; final free:8/paid:2/outside:0 |
| Free supply exhausted | Same as preceding, free capacity 5 | Final free:5/paid:2/outside:3; cost 40 |
| Shared product contention | Two identical 10-task/200-cent segments; A/B weights 3/1; prices 10/20; capacities A:10/B:10 | First proposals 8A/2B each (A wins equal remainder); shared A gives 5 each, B gives 2; each residual 3 uses B; final each 5A/5B, cost 150; total product use 10/10 |

The rational case preserves the existing
[boundary example](../examples/simulation-boundaries.example.json). Installed
monthly shares and switching/churn restrictions will supply validated edge
eligibility/limits; the kernel must not invent utility coefficients or churn.

## Remaining integration before coding the business layer

- Define concrete contract acceptance, delivery, fee/usage/refund/deadline terms
  and reserve the same customer budget only once.
- Connect product task supply to M2-02's shared compute. Contract serving must
  precede discretionary serving across products. Existing M2-02 serving rows have
  equal priority, so an explicit common resource plan/priority boundary is needed.
- Drive actual serving work from delivered demand; the engineering capacity demo
  currently supplies a fixed workload. Do not book revenue or useful-task share
  for an undelivered task.
- Persist installed shares and authored switching limits at monthly reviews;
  use committed causal records for delivered work, refunds and unserved reasons.

M2-03 remains open. The next batch implements the kernel and contract/resource
boundary with focused Mac checks against these cases and exact save continuation.
