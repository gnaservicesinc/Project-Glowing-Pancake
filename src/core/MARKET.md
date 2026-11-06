# Contracts and demand allocation

`market.hpp` owns the M2-03 business boundary. Products bind one serving project
once; segments declare task demand, money, eligibility, willingness to pay,
utility, outside utility and switching/migration parameters. It creates no
model capability or demand from downloads. All values are constant-2014 integer
units. The initial owner is a single company; rival-owned accounts and wider
regional catalogs remain later systems.

Daily work uses one copied morning snapshot. Explicit contract tasks receive
protected source priority before discretionary serving, and finite training uses
remaining compute. Contract claims sharing a product use proportional integer
apportionment with contract ID ties. The residual segment demand/budget and
product task supply feed the bounded allocation kernel. Refused tasks retry only
remaining supply/money; outside work is terminal and retains its causal reason.
The final capacity close receives actual delivered tasks, so unused demand never
creates phantom work or revenue. Every active serving project must be linked
before a market day closes, but paused setup commands may temporarily leave a
newly reserved project unlinked.

A signed contract remains pending until an explicit start command succeeds with
an available product on or before its inclusive start deadline. Closing that
deadline while still pending records one missed-start penalty. The exclusive
end date completes the last service interval without starting another day.
Cancellation records its agreed penalty once and stops future service; it does
not refund prior delivered licenses, serving support or completed work.

Fixed fees accrue as differences of cumulative monthly floors using actual
calendar days and active days. Usage charges apply only to delivered useful
tasks. For missing contract tasks, the authored service-credit fraction of that
day's fixed fee is **netted off the customer bill**; this is a service refund,
not a second charge and later duplicate cash return. Missing-task penalties are
separate supplier payables. Positive customer bills become same-day receivables;
accounts then collects/settles according to its protected-cash policy. Contracts
reserve the customer's task demand once, and their actual net fee/usage reduces
that same daily budget before discretionary allocation. Acceptance checks worst
28-day fee accrual plus maximum usage against customer money and serving tasks.
It is deliberately conservative for simultaneously outstanding obligations.

At month end, actual delivered contract/discretionary tasks and the outside
option produce one 10,000-basis-point installed-share review per segment. The
outside option does not become AI delivery. A target product's switching penalty
applies to the installed shares held by **other** products, excluding that target
and the outside base. It is not a uniform penalty on every vendor. The authored
migration limit caps discretionary target claims relative to that installed
base; contractual service and unavailable-product departures are not forced
purchases from an unavailable vendor. Utility weights remain fixed during one
daily allocation and equal the square of positive utility above threshold.

Daily contract charges, credits, penalties, segment budget/task reconciliation,
actual serving and monthly installed shares persist exactly. Reload checks
complete contract-day coverage, invoice links and actual protected/total serving
work; it reconstructs adoption from committed delivery rather than current
availability. Current availability toggles never reinterpret past eligibility.
New product/segment IDs may be added later; observed segments cannot disappear
from subsequent history. Existing segment preferences are immutable in this
primitive API; future catalog/product-launch tooling supplies explicit preference
updates rather than silently inventing coefficients.

The market supports 64 segments, 64 products, 256 contracts and 220,000
closed days. The settlement history allows two detail rows per segment per
supported day across the whole journal. The market checks the bound when
appending each day. Adoption history has its own record bound. Tasks are
bounded to one billion per segment per day, task prices to 100 million
cents and utilities to one million before squaring. Adjacent days share
unchanged contract, sale and segment rows while keeping individual dates.
