# Shared capacity and project commitments

`capacity.hpp` is the Qt-free M2-02 owning module. A business simulation optionally
owns a `CapacityState` with explicit sources, available staff, projects and daily
work history. Foundation checkpoints without capacity keep that absence.

Each source has a unique physical identity, company owner, provider, leased/owned
kind, nominal and currently effective daily output, memory and interconnect
class. Owned output names its owner as provider; leased output names a different
provider. A physical identity cannot appear twice. This prevents counting the
same slice as both leased and owned capacity. There is no resale or wholesale
operation. The opening slice lease uses memory `0` and interconnect `general`
until real hardware catalogs supply requirements; a positive memory requirement
therefore needs a source that explicitly provides it. `energy_included` is a
term describing the source; capacity does not
create a second lease/power expense. Financial billing belongs to accounts and
future owned-site energy mechanics.

A project reserves daily compute and positive assigned staff against its named
source. New commitments must fit currently effective source output and the staff
budget. Memory and interconnect must match. Training, research and evaluation
have finite work goals. Serving instead reserves its checked daily task count
multiplied by positive integer milli-CU per useful task. Its separate fixed
monthly support cost is an accounts obligation, not another compute charge.

Daily close first allocates each source to serving, then allocates the remainder
to other projects. A source outage may lower effective output beneath existing
commitments. Within each category, allocation is proportional to requested work,
capped by a finite project's remaining goal. Integer milli-CU residues use
largest remainders and project ID ties. Serving rounds to whole tasks: quota
losses determine residual-task order, and fragments that cannot fit a larger
task can serve smaller feasible tasks. Unusable fragments remain idle. This
allocation is bounded by projects per source, independent of numerical CU size.
A source cannot consume more than its effective output, and no useful-task credit
exists without its exact compute cost.

Completion records its exact work day; its output becomes usable by later owning
systems on the next morning. Cancellation ends only future staff/compute
assignments. It retains completed work and never refunds already delivered
licenses, lease fees or support already accrued. A cancelled identity cannot be
reused for a new project. Staff held by an active project remain assigned for the
whole simulated day even when an outage prevents progress.

Daily records retain effective/consumed output per source and work, useful tasks
and assigned staff per active project, including zero-progress outages. Project
causes and simulation command receipts supply explanations. Validation scans this
finite recorded history to reconcile totals, staff/output bounds, active-day
coverage, cancellation and exact completion cursors. It does not replay the full
calendar for each mutation. Mutations copy, validate and commit atomically.

Limits are 256 sources, 4,096 projects, 220,000 daily records and 3,520,000
retained project-work rows. Consecutive days share unchanged source or
project detail. Each day retains its date, physical usage and project
results for reconciliation. Sandbox throughput and memory remain part of
endurance validation.
