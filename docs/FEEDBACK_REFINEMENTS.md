# Grounded company and main-game refinement

Selected by the user on 2026-11-09 for implementation in successive milestones.
This review supersedes conflicting setup and interface assumptions in the earlier
F-02/F-03 design. The main interface is unfinished; a rendered scene, reachable
lists and passing layout checks do not establish a usable game interface.

## 1. Founding and identity

- Escape during play pauses into the menu. Escape with the menu already open
  opens exit confirmation; Cancel keeps the menu and company paused.
- Gender and pronouns are separate structured choices. Appearance is editable
  during setup and carried into the founder's actual profile. The profile shows
  the selected leg up, including Actual Genius, plus background and temperament.
- Family wealth is the first financial condition. Clearly label rich/wealthy.
  A wealthy sole founder starts alone at their parents' house/garage, with $0
  rent and space for one desk. No unsolicited trust or investor appears.
- Other routes solicit named investors, agree a cofounding partnership, or take
  employment as CEO of an existing company. Investors and donors have explicit
  offered contributions and ownership/trustee terms; unaccepted offers grant
  nothing. The CEO inherits a small office, employees, payroll and existing
  company cash, rather than personal family wealth or a finished product.
- Setup does not select a first experiment, force a first hire, shop for an
  office, or ask for difficulty. Research and product/release decisions belong
  in play. Conditions follow biography, capital, company structure and signed
  terms. A genius supplies only their own finite working capacity.
- Every opening contribution and recurring expense must be explained. No
  automatic cloud compute. Any equipment already owned or inherited must be
  disclosed; it cannot silently generate an unrelated compute allocation.

## 2. Premises, equipment and an interactive HQ

Replace the generic decorative workspace with the selected physical premises.
The starting garage should look like a garage, with room for one desk. Equipment
must be purchasable through a visible commercial supplier, then owned, delivered,
placed, moved and assigned to people. Rooms derive their function from their
installed equipment. A desk or workstation count cannot conjure furnishings.

Replace the unexplained Research annex/home-extension offers with a realtor
showing available offices/buildings in a selected city: identifiable lessor,
layout, size, price, deposit, rent and lease terms. Moving changes the actual HQ.
Offer land purchase as the other route, followed by hiring a contractor,
reviewing multiple bids and choosing an affordable design within the plot's
limits. Construction has lead time and disclosed risks of delay and cost overrun.
Owned premises pay property tax, rather than rent. Track actual equipment and
placement, not a room-label unlock substituting for physical resources.

Acceptance: a fresh solo founder can buy/place a desk and computer, inspect their
source and costs, compare actual property offers, move HQ, and assign someone
only to an available furnished desk. A home cannot acquire an unexplained rented
annex. The inherited CEO office reflects its existing roster and equipment.

## 3. Recruiting and compensation

Recruiting produces multiple applicants. Compare their abilities and backgrounds,
make offers and negotiate compensation before a person is hired. Respect accepted
terms, payroll, onboarding, workspace and the person's actual availability.
Research cannot begin by choosing an unnamed forced worker in setup. Keep the
founder's working time finite and visible when they personally do research.

Acceptance: compare applicants, negotiate and decline an offer, then hire and
assign the chosen person with agreed compensation and a physical workplace.

## 4. Research, prerequisites and compute

Show interconnected technology trees, select a node and begin research there.
Remove arbitrary research charges. Explain the required staff hours/days,
equipment, specialized laboratory and compute. Buy missing computers, racks and
specialist equipment or rent named server capacity. A desktop supplies only its
actual modest compute; an absent desktop supplies none. Work stops with an
explanation when required physical equipment, people or capacity is unavailable.

Acceptance: inspect prerequisites and connections, buy the required equipment,
allocate actual people/compute and see progress follow their work. Every CU/day
traces to an owned working device/rack or a signed paid rental; no free 100 CU/day.

## 5. Main interface, devices and maps

Build a game interface with a coherent visual language. Style all visible
controls to fit it; stock Qt-looking buttons, selectors and lists are unfinished
presentation. A laptop should read as a laptop interface with applications, not
an unstylized collection of management lists. Preserve keyboard access, readable
units, text scaling and pause while improving presentation and interactions.
The State view must show the selected state's actual geographic outline and
context, with interactive locations and readable briefings, rather than only
scattered county markers. Continue the player's broader UI review after the
missing interactions are implemented.

## Development reset and verification policy

Development saves have no compatibility guarantee. Do not implement migration
or old-save support as a feature gate. Verify fresh saves within the current
build. Development builds include a one-click **DEV · Reset local data** action
for human testers, including when a broken save blocks normal play. It clears
this game's local saves, autosaves, cached rules, setup/navigation drafts and
settings and returns to a clean New Game entry. It does not delete files exported
outside the game's local data directory. All current builds, including optimized Release builds used by human testers,
include this action. **Before shipping v1.0, remove the menu action and its
callable implementation**, and verify that shipped packages contain neither.
No separate build flag is required during current development.

Use focused Mac checks for each milestone. Do not infer player acceptance from
engineering checks or reopen broad release qualification for each change.
