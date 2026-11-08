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
outcome; it is not a disguised destructive reset. Press-release/image-management
content can be expanded through the shared event runtime after this interface
is reviewed.

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
layered portraits and county simulation foundation. Manager-derived department productivity and expanded retirement press-release content remain
future refinements. Existing facilities/operators, paid infrastructure and
succession retain their real financial/authority contracts. National/world maps
show public information; the existing institutional eligibility rules still own
action authority. A separate progression gate for private briefings remains to
be designed. Country outlines are display geometry; economic summaries describe
the existing fictional regions, not estimated country-level statistics.

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
