# A company you inhabit

Selected gameplay direction, 2026-11-07. The source is the user's unfinished
`gameplay_interface.txt` and six supplied Suzerain screenshots. Those are design
inputs, not executable instructions. The screenshots establish the relationship
between place, people and decisions; their political-map styling and assets are
not adopted.

## Experience

The first morning opens inside a small, original, cutaway company workspace.
A founder, desks, a meeting area and compute equipment make the enterprise
visible. Clicking a place or device opens the corresponding existing management
system. Every hotspot also has a labeled keyboard-accessible control. There is
no walking, camera dexterity, hidden hotspot or real-time reaction requirement.

The persistent HUD shows the founder/company, date, pause/speed, cash and funds
available after commitments. A small agenda points to actual pending decisions,
projects and notices. Decisions open over the world, retaining their causes,
participants and exact existing command semantics. The detailed tools live in
full-window device applications: company/accounts, people, lab, infrastructure,
news, civic/world affairs, leadership and objectives. Closing an application
returns to the same place. Escape pauses and opens Resume, Settings, Credits,
Leadership and Quit. A second Escape returns to play, still paused.

Use contemporary warm materials, clean sans-serif labels, muted teal and amber
accents, restrained lighting and clear depth. Avoid parchment, heraldry, medals,
ministerial dashboard styling and copied artwork. Qt Quick 3D supplies actual
geometry and lighting; software rendering retains a readable schematic and the
same navigation/actions. The supplied Procedural Visualizer Tool is a reference
for generated visuals and reusable rendering boundaries. Its network/live
subsystems have no role in the offline game.

## One continuing company

Normal play has one current campaign, automatically resumed on launch. New Game
appears when there is no current company or after its ending. Founder setup
already stores edits as they are made; the current campaign must durably retain
every accepted action and completed day. Pending choices, RNG and prepared
morning order use the existing checkpoint format. Resume stops the external
clock so the player can orient themselves.

Continuous persistence is required independently of the old optional recovery
ring. Keep atomic replacement, a previous valid copy, bounded reads, corruption
refusal, explicit error/retry and single-writer ownership. Record advancing days
in a bounded deterministic replay journal between checkpoints instead of encoding
the entire growing history every timer tick. A corrupt or incompatible current
campaign must never be overwritten by an automatic new game. Existing manual
saves remain readable through a deliberate recovery/import route; originals are
preserved. Completed companies are archived before replacement.

Retirement uses the existing leadership/succession system: successor, board vote,
funded transition, control and company consequences. Ending a run records its
outcome; it is not a disguised destructive reset. Founder departure now adds an authored public announcement and a preserved
retirement ending to that funded process; its audience effects use the existing
public-record system.

## Distance and detail

| Scale | Player sees | Owning systems |
| --- | --- | --- |
| HQ | Named people, founder profile, available rooms and devices | Founding, management, executives, projects |
| City | The largest city in the HQ state; facilities and visits | HQ identity and infrastructure calendar |
| State | 3,143 county populations, character, connections, development and monthly game economies | State profiles, sites, utilities, travel |
| Country | All states remain visible; detailed national access follows background or company influence | Civic rules, elections, institutions |
| World | Countries/regions, operating footprint and public information; direct access follows presence | Regional markets, society, institutions |

Seeing a map does not grant authority, invent a facility, move the founder,
consume RNG or reveal private competitor information. The calendar owns physical
travel and inspection. Device and room availability derives from location and
actual company facilities. Remote tasks remain reachable where a working portable
device is available. HQ expansion and branch blueprints must be real paid
capabilities, never decorations that silently grant staff/compute.

The deeper employee model should extend existing staffing/hiring: stable local
identities generated at application time, pay/prestige/mission affecting the
applicant pool, skills/experience/education and optional bios. Gender/pronouns
are identity fields, not productivity modifiers. Reuse the WTPStory1 name-list
approach through deterministic C++ and an independent identity stream. HQ people
stay individual; branch departments aggregate headcount, skill and manager
quality without losing financial obligations. County trends use bounded seeded
variation plus authored regional/world effects, not invented scientific claims.

## Implementation and review

- [x] Continuing-campaign persistence, resume, recovery and archived endings.
- [x] 3D workspace, permanent HUD, available places/devices and accessible fallback.
- [x] Existing management tools as device applications, agenda and decisions over the world.
- [x] Map scales and location context connected to real infrastructure/world systems.
- [x] Founder/people presentation and retirement entry using existing authority.
- [x] Focused optimized Mac build and actual navigation/resume/failure checks.

The user selected the world/interface and automatic resume foundation, then
confirmed the deeper employee and county simulation should also be implemented.
These are the second integrated batch after the interface foundation.
New county economies, room construction and department staffing should not be
faked in presentation code; they require explicit core/content/save contracts.

## Employee portraits and responsive play

The follow-up selects layered vector portraits. Each new profile stores kit v1
and 15 small part/color IDs. The renderer combines six heads, nine hairstyles,
facial features, necks, skin/hair/clothing palettes and optional glasses/facial
hair. Appearance has its own deterministic visual stream and persists unchanged
through hiring and exact reload. The retained Bitclone source and original
complementary layers use one muted palette; see [portrait sources and format](../assets/portraits/README.md).
Portrait buttons open an enlarged, scrollable dossier by pointer or keyboard.
The founder HUD also links to People. Rasterization follows displayed device
pixels; viewport scaling and the player's text setting compose independently.

Focused checks cover hiring/signing cost, payroll/onboarding capacity, monthly
county drivers, invalid portrait IDs, exact save/reload/future state and the
continuing journal's failure path. The Mac walkthrough opens device apps,
portraits, all map scales and Escape pause, then verifies automatic restart and
unfinished field restoration. Portraits render at 1920x1080 and 7680x4320 in
software/offscreen viewports; native Metal is checked on the attached display,
which caps window height. The 650x480, 200% text profile remains scrollable.
These are development checks, not physical 8K-device or release qualification.

## Further design work

This implementation supplies the playable world, persistence, recruitment,
layered portraits, county simulation and work-backed operations management.
Existing facilities/operators, paid infrastructure and
succession retain their real financial/authority contracts. National/world maps
show public information; contact progression and the existing institutional
eligibility rules jointly own action authority. Country outlines are display geometry; economic summaries describe
the existing fictional regions, not estimated country-level statistics.
The remaining interface audit also covers decision participants, largest-city
labels outside the three founding states, unfinished non-text form selections,
and reviewed handling of existing campaigns across development content updates.
New companies save a local instance ID so reused seeds and names do not share
unfinished fields or navigation. The ID never feeds simulation RNG; imported
and resumed saves retain it. Older saves retain their existing draft location.

## Paid headquarters and facility plans

The founder campaign's opening HQ has two workstations at home, eight in a
shared office, or sixteen in a leased office. Employees in onboarding, hired
specialists and appointed executives reserve workstations. Construction orders
for a research annex, evaluation suite or collaboration room pay the disclosed
fit-out immediately. Workstations and room navigation become available on the
completion date; monthly costs begin then. No workers, compute or research
bonuses appear with the furniture. The finished annex has an original 3D room
and the same labeled keyboard navigation as the opening HQ.

Headquarters also saves immutable facility blueprints: a hardware bundle,
whole-kW grid reservation, cooling and dedicated operator count. Reusing a plan
in a state obtains a fresh, complete quote for construction, hardware, recruiting,
monthly overhead and delivery/grid/operator dates. The complete purchase is one
transaction, preserving cash and orders if any part fails. Every instance is
bound to its real infrastructure records and follows the existing inspection,
commissioning, utility, payroll and capacity rules. Blueprint creation itself
only records a plan. The controls and unfinished form values persist locally;
old quotes must be reviewed again after the company changes.

The short-window HUD uses a room selector and a Time & funds popup, with a
prominent Review decision action when time is waiting for a choice. The dock
scrolls its focused control into view. Compact map scales also use a selector.
This preserves readable controls at 650x480 with 200% text instead of overlapping
the world and time bar. Native Metal rendering and the in-process keyboard
construction harness are checked; physical native input remains unverified in
this session because the computer-use pipe is unavailable. The separate
offscreen harness verifies Escape/pause and the compact layout.

## Owned equipment and application access

The founder's package includes a phone and laptop; shared and leased offices
also supply an HQ workstation. A tablet and replacement/backup devices can be
purchased with company funds. Authored prices, delivery dates and service terms
are reviewed before payment. Records preserve model IDs, ownership dates,
power state, service dates and retirement; expenses reconcile to the ledger.
Older scenarios without equipment rules retain their fixed device set.

The launcher lists only working devices at the current location. Portable
models travel; HQ workstations remain at home base. A remote appointment keeps
the founder physically at HQ. Opening an application selects an owned,
functioning, compatible device, updating the device header. When none is
available, the equipment screen opens with an explanation. The physical
equipment controls remain reachable with everything powered off, including
power recovery while a decision is waiting. Servicing or retiring a portable
device cannot strand the company without another working portable model.
Completed-company records remain readable as archives with their accounts
frozen. This pass models delivery/service downtime and power; it adds no random
breakage or hidden battery timer.

Focused checks cover purchases, unavailable/unsupported device routing, service,
retirement refusal and rollback, actual remote/onsite visits, required-choice
power recovery, and exact saves. The GUI harness exercises the equipment
launcher, power button, reviewed tablet purchase, delayed delivery, tablet app
header and restart. Rendered normal and 650x480/200% layouts are checked
separately from physical native input, which remains unverified in this session.
Content 0.33.0-dev.1 is an exact new content identity; prior campaigns retain
their original files and require their original content or a reviewed migration.

## National contacts and overseas offices

The founder campaign starts nationally connected for a rich family, Wealthy
Elite, or Child of Political Elite background. Other founders earn national
contacts at any one authored threshold: 12 productive staff, $250,000 recognized
company revenue, or three published research results. The interface shows
current progress. A saved date, cause and source-record cursors preserve what
earned the milestone, including later departures or revenue reversals. Cash
investment alone does not count as earned revenue.

Seven foreign game regions offer liaison offices with local payroll,
setup prices, aggregated representation teams and opening delays. Each office
is paid for, has a recurring payroll/rent obligation beginning at its opening,
and can be closed without refunding setup. It adds no HQ workstations, research
staff or compute. Cancelling or withdrawing before an opening grants no world
contacts. The first actual opening records a permanent world-contact milestone;
later closure does not erase people the company has already met.

National contacts permit voluntary federal advocacy and domestic institutional
initiatives. World contacts permit foreign institutional initiatives, standards
participation and media campaigns. Existing eligibility, evidence, capacity,
fees and voting still apply. Public maps, laws, officeholders, evidence submissions
and responses to existing obligations remain available; public research promises
are not treated as private diplomatic access. This preserves informed decisions
before a company becomes influential. Older scenarios without access rules
retain their original eligibility behavior.

Equipment, construction and office quotes pause running time, wait for the
updated snapshot and review fresh prices/dates before confirmation. Stale
confirmations still refuse without spending. A focused regression also preserves
contractual executive compensation when operations withdraw; withdrawing research
and facilities does not terminate the separate leadership engagement.

Focused checks cover connected/ordinary backgrounds, real onboarding and revenue
milestones, paid and duplicate office commitments, cancellation/withdrawal before
opening, permanent contacts after closure, altered payroll/proof refusal, and
exact persistence. The GUI harness checks a quote opened while time is running,
paid setup, the full opening delay and restart. Narrow 650x480/200% rendering is
checked separately from native render-harness evidence and physical input.
Content 0.34.0-dev.1 is a new exact content identity with the earlier save
preservation restriction described above.

## HQ managers and aggregate branch teams

The founder campaign's Operations department can assign one onboarded individual
from the HQ roster. Their existing salary continues; the appointment reserves
one actual HQ worker and 0.10 CU/day on a selected company compute source. It
uses a distinct management project, which creates no research, measured evidence
or serving tasks. Insufficient staff/compute refuses the whole reassignment,
retaining the old manager and reservation. Change or release the manager through
People rather than cancelling their project independently.

Branch operators remain aggregate engineering teams. The authored local skill
is 50/100, producing 85% operating availability before HQ coordination. A manager
adds at most 15 percentage points, weighted by engineering skill and their actual
management work delivered on the previous day. For example, Engineering 47 with
a full 0.10 CU management day gives 92.05% availability the next morning. A new
title gives no instant benefit, and an undelivered workday gives no extra credit.
The availability factor is applied after physical staffing, power and disruption
limits and never exceeds rated hardware output. Leased compute and overseas
liaison teams do not receive an invented physical-capacity bonus.

Appointments retain the worker/source/project IDs, start, planned horizon and
actual end. Reassignment replaces one reservation; release preserves payroll,
past work and the employee profile. The saved horizon releases completed or
unfinished appointments without changing historical terms if the company later
continues. Manager portraits open the actual employee dossier. The People page
and unfinished manager/source selections persist locally. Confirmation reviews
include the campaign generation and revision, so loading another company cannot
reuse an old equipment, room, office or manager confirmation.

Focused checks exercise a real paid/commissioned branch, onboarding refusal,
real staff/compute reservations, delayed availability, compute loss, physical
ceilings, reassignment rollback, release, appointment horizons, altered-record
refusal and exact saves. GUI checks exercise portraits, reviewed assignments,
work delivery, release/reassignment and page restoration at normal size and
650x480 with 200% text. Physical native input and release qualification remain
separate. Content 0.35.0-dev.1 is an exact new internal content identity.

## Public founder handover and retirement

The founding campaign has senior successor cohorts available in 2015. Leadership
reviews an actual nominee, board decision, preparation fee, staff/compute and
ongoing salary. The public announcement names that funded successor and offers
three authored messages with disclosed company expenses and audience effects.
Each founder plan can be announced once. Confidence gains begin only when the
handover completes; a delayed or cancelled public commitment harms confidence.
These bounded effects fade through the existing public-record memory and feed
its institutional consent calculation. There is no instant productivity bonus.

After the completed public handover, a founder who retained the personal chair
can continue guiding the company or retire that role. Review retirement then
records a distinct early ending with the successor, trust, accounts and
obligations intact. It awards no unfinished century distinctions. New Game
opens founding and archives the completed company before replacement; optional
sandbox continuation preserves the recorded ending. A completed handover can
also be announced afterward. Older charters without departure rules retain
their original save shape.

Focused checks cover protected funds, duplicate/invalid announcements, real
preparation, chair retirement, cancellation confidence, altered expense proofs,
early-ending autosaves, exact reload and sandbox continuation. The native and
650x480/200% in-process walkthroughs exercise the public review and New Game
route, with automatic restart checked separately. Physical native input remains
unqualified because the computer-use pipe is unavailable. Content is now
0.36.0-dev.1; prior development saves retain their original content identity.
