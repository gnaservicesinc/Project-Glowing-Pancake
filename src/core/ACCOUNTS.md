# Accounts and obligations

`accounts.hpp` is the Qt-free M2-01 owning module. The simulation optionally owns
an `AccountsState`; engineering foundation checkpoints without accounts retain
that explicit absence. Scenario creation supplies opening capital, cap-table
holdings and recurring terms. Loading an older foundation checkpoint does not
invent a historical ledger. The simulation's cash field must equal journal cash.

Each positive posting names one debit and credit, a stable identity/key, day,
owner and cause. Cash, receivables, assets and expenses are debit-positive;
accumulated depreciation, payables, debt, equity and revenue are credit-positive.
Reports derive balances and profit from postings. Reservations remain simulation
commitments: creating or cancelling one does not recognize an expense. Consuming
one recognizes the delivered purchase once. Invoices recognize revenue or expense
separately from subsequent cash collection/payment. Debt and equity proceeds do
not create product revenue. Generic engineering flow cash transfers use equity.

Recurring terms accrue daily using the difference between successive cumulative
floors of `monthly_cents * active_days / actual_days_in_month`. Mid-month starts
and exclusive ends charge only active days; a full month recognizes exactly its
authored monthly amount. Accrual creates a payable due at that month's end.
Ending a recurring service preserves prior accruals. Its exclusive end may equal
its start when cancellation happens before any service is consumed.
Debt terms specify a **monthly** basis-point rate. Daily interest uses the current
principal and the actual calendar-month denominator, carrying fractional cents
within that month; the remainder resets at the next month. Interest creates a
payable. Principal is repaid only by an explicit validated command; maturity is
reported by cash projections and does not create an implicit cash overdraft.

At daily close, agreed due receivables collect before due payables pay. Each
category sorts by `(due_date, unique_invoice_id)`. Payment uses only unreserved
cash and may partially settle a liability; an unfunded remainder persists.
Collection currently treats an issued receivable as collectible on its due date;
customer uncertainty and late-payment policy belong to later contract mechanics.
A close must name exactly the next unclosed day. Mutations copy, validate and
commit atomically, so stale or rejected operations cannot leave partial postings.
Transaction keys cannot be reused, and public callers cannot preempt the reserved
opening, invoice, debt, accrual or automated-settlement namespaces.

Equity investments allocate the purchased basis points and dilute the existing
fully diluted holdings pro rata. Floors are followed by largest-remainder
allocation, with holder ID breaking ties. The total remains 10,000 basis points
and the existing reserved pool dilutes with the other holdings. Board seats,
vesting, negotiated option grants, credit-line offers and merger policy remain
later mechanics.

`project_cash` closes a copy through its requested date **inclusively**, returning
cash, available cash, unpaid due payables and due principal separately. It consumes
no authoritative RNG and changes no live cursor. It assumes the current terms
and agreed invoice collections; it does not forecast future customer acquisition.

Reload validation checks canonical posting chronology and individual invoice and
debt reconciliation, then reconstructs fixed-obligation accruals and monthly debt
interest residues through the saved close cursor. Changing terms or resetting an
interest cursor cannot silently skip recognized charges. Daily settlement visits
only unpaid and future-due invoices. Settled records retain their identities and
amounts for audit, including when the open register is empty. Ledger reports use
checked balances derived as postings append and rebuilt from postings on load.
Checkpoint copies share historical pages and isolate changes on write.

The history budget allows 16 postings and four invoices per supported day
across the retained journal, for up to 3,520,000 postings and 880,000 invoices.
The open register remains capped at 8,192 unpaid or future-due invoices.
There are separate bounds of 256 recurring terms, debts and holders and
36,524 days per cash projection. Saves retain the original posting and
invoice order. The scored century and sandbox have separate validation runs.
