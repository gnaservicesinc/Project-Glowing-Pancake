# Domestic infrastructure

Operations opens the 51-jurisdiction map and a searchable, keyboard-selectable
state list. Select a state, optionally pin it for comparison, and review a named
site's price, incentive, grid slot, construction date and equipment power limit.
Buy racks and hire dedicated operators from **Owned sites**, then arrange an
inspection in **Calendar**. **Next site milestone** stops at actual future dates
and stays interruptible by ordinary campaign decisions. Active racks appear as
compute sources in the Lab and deployment controls. Use the opening lease
notice after moving work and services that still rely on it.

The geometry is a fixed local display asset. Its catalog SHA-256 is checked
before rendering; the complete list works if geometry cannot be loaded. The
[asset source, preparation and terms](../../assets/maps/README.md) record the
2024 Census input, transformation and acknowledgement. State prices, hiring,
water, power and travel are authored game assumptions in constant 2014 cents.
They are independent of the boundaries.

## Physical and financial contract

- A paid build reserves whole kW within a finite state allotment. Each state
  schedules its next connection slot after the last reserved slot or today's
  date, whichever is later. Site closure releases watts; it does not move other
  promised dates or reclaim a canceled crew slot. Property/overhead begin on
  building completion. The bounded incentive reduces capitalized cost once.
- One rack order creates at most one owner-qualified physical source. Delivery,
  the completed inspection, grid connection, assigned operators and enough
  power are all required. The site assigns its dedicated operators in delivery
  order with ID tie breaks. They do not add Lab staff. A rack's exact memory and
  interconnect class are checked against project requirements. The reference
  rack supplies 100 CU/day in the same units as leased compute. The fabric rack
  requires research and stronger connectivity; the wide table recipe requires
  compatible memory and fabric. A working rack can outlive its book life.
- Grid reservation and the chosen power budget limit facility watts. For
  evaporative cooling, the daily water allotment adds a facility watt limit.
  Dividing by fixed-point PUE gives IT watts. Staffed racks draw idle power
  first; usable compute is rounded down in proportion to the remaining power
  above idle. If idle load cannot be supported, the staffed group is powered
  off. Existing reservations can slow; new allocations must fit usable supply.
- Capacity closes before the meter. For each powered rack, the actual daily
  energy is `24 × PUE × (idle W + (active W − idle W) × used CU / nominal CU)`.
  Fractions are carried in integer units. Water, carbon and utility cents use
  actual energy; PUE is applied once. The meter does not charge the opening
  lease or other leased sources. Site months retain energy, water, carbon,
  electricity, water charges and fixed operating costs.
- Property, wages and maintenance accrue over actual calendar month days.
  Metering and operating costs form one owned-operations accrual per company
  day and a monthly payable. Cash forecasts include committed fixed terms and
  a conservative full facility energy ceiling, including known lease notice
  dates. Capital purchases enter assets; monthly depreciation uses cumulative
  straight-line entitlement, with the remaining earned amount booked on
  disposal. Resale is capped at a fraction of remaining book value; orders
  canceled before delivery and unfinished sites pay no resale. A protected
  customer contract must be resolved or moved before retiring its source.
- On-site visits reserve the executive calendar for outbound travel, inspection
  and return. Remote appointments reserve the visit day. Delegation pays a
  separate fee and does not occupy the executive. A conflicting reservation is
  refused. Cancellation before departure/work releases the slot while keeping
  the paid fee. Physical location never gates ordinary management.

The same typed commands serve previews and the campaign worker. Irreversible
commitments use the local autosave policy. Restore validates paid purchases,
connection and staffing dates, physical source IDs, booked terms, causal
notices, depreciation and actual metering from the saved work intervals, then
checks the exact scenario catalog and headquarters. Daily processing appends
bounded records; it does not replay previous meters. Bounds include 50 sites,
four live orders/site, 200 historical orders, 400 hire batches, 512 appointments
and 4,096 notices. Long campaign storage qualification is M8 work.

## Mac checks

```sh
ctest --test-dir build/release -R '^(core.infrastructure|runtime.infrastructure|tools.domestic-map)$' --output-on-failure
build/release/bin/pgp-game --smoke-test --infrastructure-smoke --text-scale 200 \
  --portable-root "$PWD/.work/infrastructure-check" \
  --screenshot "$PWD/.work/infrastructure-check.png"
```

Use a fresh scratch root for the explicit app check. It chooses a state by
pointer and keyboard, books paid construction/hardware/staff, refuses a calendar
conflict, licenses data while traveling, runs owned Lab work, meters curtailed
energy, ends the opening lease and reloads the exact campaign. The tests also
cover a water-limited case, stale commands, real fabric compatibility, pending
and metered saves, tamper rejection and resale. These are engineering checks.
The [regional market and wholesale controls](REGIONAL_MARKETS.md) build on
these owned sites. Hazards and resilience follow in M5-03.
