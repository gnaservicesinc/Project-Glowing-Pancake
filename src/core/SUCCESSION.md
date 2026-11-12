# Leadership succession and the founder legacy

M7-02 adds Leadership to the two century scenarios. The player guides the
institution through changing executives. Ownership, customers, models, staff,
sites, board commitments, debts and existing bills retain their actual records
through a handover.

## Charter and people

The authored management catalog has an optional succession charter. The stock
century charter limits an executive term to 20 calendar years and retirement
to age 70, whichever arrives first. The founding director was born in 1980 and
has a first retirement deadline of 2034-01-01. The personal founder chair role
ends by age 80. These are editable fictional governance terms. Dates use the
existing Gregorian calendar and clamp a term anniversary to the month's last
valid day when necessary.

Two leadership tracks offer named people from distinct birth cohorts. The
interface shows each person's birth year, priorities, preparation fee and
monthly salary. A handover requires a candidate aged 35–60 at the reviewed
date. Cohorts recur every 20 years with separate person identities. A person
can return while eligible and remains subject to their original birth year.
Neither a new term nor a repeated name restarts an individual's career clock.
The compiler checks that a campaign's first retirement has an eligible cohort.

### An ASI chief executive

The optional `asi` charter adds the company's own independently established ASI
systems to the candidate list. AGI and research forecasts do not qualify. System
IDs resolve only within this company's immutable Frontier deployment bundles.
Qualification is read as of the nomination date, preserving its independent
control bound in the plan and eventual executive term. Save validation compares
those records with the dated Frontier evidence.

An ASI uses ordinary board approval and paid transition exercises. On appointment
it takes the normal chief executive seat and its authored finite mandate.
It has no human age or portrait. Contracted hosting and human oversight create
ordinary recurring company expenses from appointment until the term ends.
Human caretaker cohorts remain the automatic fallback at a mandate boundary.

Resignation coverage can lead with the ASI appointment or scrutiny over its
independent control evidence. Both the announcement and weak-control reaction
use authored audience effects. The original PR claim and its dated financial/
research assessment remain in the story. Later company performance cannot
rewrite that nomination evidence.

The charter warns 365 days before a term ends. Private notices retain their
source, deadline and acknowledgment, and stop the normal clock and milestone
driver. They can be read through Leadership or the required-decision banner.
The founding institution continues after personal retirement.

## Reviewed handover

A nomination names a person, date and actual preparation allocation. It opens
a 14-day board window. Supporting or opposing a nomination uses the same seats,
cash positions and majority rules as ordinary corporate decisions. The preview
shows the resulting ballots and expense before confirmation. Ignored proposals
expire. Only one proposed or funded successor plan can be active.

An approved transition pays its authored preparation fee and reserves at least
90 days of finite exercise work. The default minimum work is 900 CU, with one
or more staff and positive spare compute. A larger allocation still performs
the minimum calendar duration. Funding rechecks unreserved cash and the
charter's $100,000 floor. Existing service and project commitments retain their
staff and physical compute reservations.

The prepared morning installs the successor on the reviewed date after the
actual exercises finish. A delayed project produces a notice and retains the
incumbent's existing retirement limit. Cancellation stops remaining transition
work and retains spent fees as expenses. A stopped project or changed incumbent
invalidates its old plan. A missing ready plan at retirement invokes the
charter's paid caretaker appointment, with a five-year term bounded by the same
age limit. Rivals use this same career and salary logic and retain their
original authored founder names.

## Chair, board and compensation

At the first planned handover, the founder may move to chair or to the trust's
institutional representation. A personal chair can retire early. On the
retirement date it automatically becomes an institutional trust representation.
The stock board changes the founder's two executive seats to one chair/trust
seat and assigns one seat to the current chief executive. These actual rights
affect later financing, dilution and acquisition votes. Saved votes retain their
leadership-control cursor so later succession cannot rewrite earlier ballots.

The founder's initial executive salary is $5,000/month. The chair stipend is
$2,000/month. A new chief executive has their track's separately disclosed
salary. Preparation and salary use ordinary company expenses and recurring
obligations in constant 2014 cents. Ending a role closes its future recurring
term while retaining already accrued bills. The appointment replaces the
executive office and preserves the operational team allocator.

## Retained beneficial ownership

The founder trust keeps its original holder identity and the ownership actually
left after financing and other corporate actions. Succession changes neither
the cap table nor the original pool. A successor receives their agreed salary.
The founder's retained personal cash is credited only from paid founder salary
or chair invoices, using an incremental ledger cursor. Unpaid compensation
remains a company obligation.

`runtime::founder_legacy` reports retained beneficial claims and current book
wealth. It combines the trust's paid cash and ownership positions, then deducts
its authored personal liabilities. Company cash is included within equity value.
For the parent, book value uses the existing consolidated group report, which
eliminates internal acquisition and funding assets.

After an acquisition, the selling founder retains their actual cash settlement
and their share of the parent's pooled seller holding. The beneficial fraction
uses the current parent holding and the saved settlement proportions, with one
valuation division. Later parent dilution affects that claim once. A wholly
owned target supplies no second direct founder stake. These are book reports.
M7-03 supplies terminal objective prices and audited profit adjustments.

## Ownership and authoring

`succession.cpp` owns nominations, votes, funded exercises, career dates and
role changes. `succession_validation.cpp` reconciles dates, people, source work,
fees, salaries, beneficial identity, paid cash and control history. Runtime owns
cross-company legacy queries. Session owns actual deadline stops, and Qt Quick
uses immutable snapshots and reviewed commands.

Studio exposes charter values, cohort names, compensation and existing board
references through the shared forms and catalog. A reviewed board rename also
updates its charter reference. The successor's `leadership.ceo` seat is reserved.
The registered boolean facts are `company.has_succession_charter`,
`company.leadership_handover_today` and `company.founder_is_chair`.

Old content omits the optional charter and continuation. Older board votes omit
their default zero leadership cursor. New saves preserve proposed/funded plans,
projects, dated terms, role history, notices and founder cash. Record limits are
128 plans, 256 terms and 512 controls/notices. Eight opening board members,
eight financed seats and the successor fit 17 recorded ballots. Whole-campaign balance and
500-year resource/history behavior retain M8's separate endurance work.

## Focused checks

`runtime.succession` exercises actual service continuity, board-approved paid
preparation, exact handover/retirement dates, cancelled and expired plans,
protected cash/compute, player and rival caretakers, conserved acquisition
consideration and exact save reopening, including the maximum 17-member board.
It also refuses forged founder cash
and invalid charters. The 2035/2114/2514 cohort checks verify future candidate
availability and ages.

The explicit native walkthrough uses a 2027 opening with a near-retirement
founder and an earlier chair deadline, while retaining the actual transition,
payroll and board rules. At 100% and 200% text, Metal controls nominate, preview
and pay for a real transition, reach the dated handover, retire the personal
chair, cancel the next plan and reload the exact save. It uses the game's
60-days-per-second clock setting.

Prepare an isolated fixture with
`pgp-succession-test --smoke-fixture /path/to/new-scratch-directory`.
Run `pgp-game --smoke-test --leadership-smoke` with that directory's
`--content` collection, `--scenario scenario.century_frontier` and matching
`--portable-root`.
