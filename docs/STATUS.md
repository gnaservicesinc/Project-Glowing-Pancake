# Project status and next handoff

Updated 2026-11-10

## Implementation progress

**The initial skeleton phase is complete and closed for human feedback.**

The user accepted the current core as the starting point for the next phase
and directed the initial implementation pass to wrap up. The gameplay
checkpoint was `d1f2bd3`, with internal content 0.29.0-dev.1. The current
immersive redesign advances content to **0.51.0-dev.1**, generation **52**.

The detailed game design remains a reference for feedback and future work.
Working-condition programs and other deeper mechanics previously proposed as
next tasks are candidates for that review. The initial implementation queue
is closed.

## Current human review: grounded founding and unfinished main game

The expanded F-05 [immersive company plan](IMMERSIVE_COMPANY_PLAN.md) is now the
selected ongoing work. The first batch separates the childhood intro from
company formation: appearance leads to an unemployed eighteen-year-old's saved
home desktop, with legal, investor and lab-application routes. Explicit personal
investment leaves the remainder in the personal account. Birth and incorporation
ceremonies, narrated choice consequences and an expanded portrait editor are
implemented. The family home provides three, four or six furnishing rooms depending on
background. The realtor can move the company home after settling its lease.
A founder can choose a self-funded opening and adopt optional bylaws with a
funded employee program and limits on board approvals. Redwood Mutual offers a
startup loan with a minimum personal contribution. Signing draws company debt
with dated interest and maturity under the existing accounts rules.
The home inbox persists lab application replies, recruiter invitations and
funding pitches. You can accept, defer or decline each active offer. Accepted
terms reach the appropriate incorporation or appointment ceremony. Their actual
cash, seats and equity take effect at signing.
Focused checks cover draft restart, write failure and the inherited company. The
Release software walkthrough covers a rejected application and an investment
offer through defer, accept and withdraw at 650 × 480 with 200% text.

Furnishing now supports illustrated object pickup, translucent red/green
footprints, rotation and cancellation using the same core placement validation.
The catalogue includes the requested computer tiers, issued laptops, rack-dependent
clients and office amenities. Fictional supplier storefronts expose their
prices, delivery and amenity mood benefits. Team creativity counts each distinct trait once;
compatibility supplies a bounded coordination benefit. Hiring and research
previews show the real team values, and sufficient creativity can bypass research
prerequisites without granting free equipment or compute. Each shared trait
contributes toward bounded compatibility. A new unshared trait preserves
existing matches. Employees show a mood score based on salary, company and
CEO reputation, mission and duties, coworker affinity, temperament, and active
HQ amenities. Installed amenity types benefit employees with assigned desks.
All 60 founder research topics have expected years and difficulty values. The
research tree shows each part of the creativity threshold. Selected frontier
research uses topic pacing before the shared frontier date, with seeded world
conditions and physical requirements still enforced.

HQ conversations use actual room and desk assignments. The founder can exchange
details with an onboarded employee and then use a tablet or another working
device to revisit the conversation. Dated mood and equipment replies persist,
and available company laptops can be issued from the same view.
Opening recruiter, application and funding threads now carry into company
Messages with their original message and final signed, declined, rejected or
unsigned outcome. Follow-up replies confirm the opening agreement and the
organization’s current board seat. Unsigned opening offers close at formation.
Focused checks cover agreement validation, device/topic refusals and reload.
The Release software walkthrough covers contact exchange, the visible latest
reply in both views at 650 × 480 and 200% text, laptop issuance and fresh reload.
The home walkthrough also checks the opening conversations and a saved company
reply at the same compact size and text scale.

My life now provides three personal home listings, furniture and gallery art.
Savings and paid salary fund purchases and monthly upkeep. Unpaid upkeep stays
in personal liabilities and must be paid before discretionary purchases resume.
Homes and delivered interiors can be sold at disclosed resale prices. Selling a
home stores its contents. Illustrated room placement supports rotation,
valid/blocked previews and keyboard coordinates. Current book wealth and the
terminal valuation include private assets and personal liabilities separately
from company equity. Content **0.47.0-dev.1**, generation **48**, records the
catalogue. Focused checks cover arrears, sale proceeds, refused placements,
historical cutoffs and fresh reload. Content review passes 24 prepared opening
roundtrips without compiler lints.
The bundled Release software walkthrough checks home and artwork purchases,
delivery, valid/blocked previews, rotation, both resale actions and reload.
At 650 × 480 with 200% text, the whole floor plan fits and sale terms scroll
within the dialog.

Hosted gatherings now connect personal spending and known contacts. My life
offers dinners, house parties, formal receptions and personally funded office
parties with quoted costs, guest limits, notice and cancellation refunds.
The saved calendar records replies and attendance, offers advancement to the
next gathering and opens an evening report. Attending employees receive the
strongest recent gathering’s mood contribution for its authored duration.
Travel or an on-site appointment makes the host miss the event. Moving HQ
refunds an undelivered office booking, and a pending home event blocks sale of
its venue. Content **0.48.0-dev.1**, generation **49**, records the packages.
Focused checks cover known contacts, duplicate/invalid invitations, personal
payment, refunds, venue changes, attendance, mood expiry and reload. Content
review passes 24 prepared opening/save roundtrips without compiler lints.
The bundled Release software walkthrough covers selecting known guests,
reviewing payment, cancellation, rebooking, advancing to the event, recorded
attendance and the evening report at 650 × 480 with 200% text. Employee messaging,
the expanded mood breakdown, laptop issuance and fresh reload also pass.

Focused identity/investment, headquarters, creative-team and workplace-research
checks pass, including refusal and fresh reload. Content review validates 24
prepared openings/save roundtrips without compiler lints. The rendered software
intro and furnishing checks pass at normal/compact sizes. The native Metal
furnishing walkthrough also passes through valid/blocked previews, visible
rotation, cancellation, relocation and a compact 200% text layout. The software
run covers the R shortcut; native terminal activation prevented shortcut testing
there, so the visible rotation control was used. The bank walkthrough covers
a declined application, the signed loan and a fresh reload. The simulation
check also covers accrued interest and repayment.
The employee profile walkthrough verifies a visible mood score at 200% text.
Simulation checks cover stored and installed amenities, duplicate types, power,
desk access and the same mood after reloading.
Research checks cover a real prerequisite lowering the threshold, an early start
by a mixed team, refusal for a solo team, unchanged frontier conditions and an
exact reload of the early project. The Lab walkthrough verifies the early start
calculation and the compact research tree.
Employee conversations now offer reviewed dismissal in person or from a known
contact's compatible device. It pays one authored month of actual salary from
company cash, ends that employee's payroll, returns equipment assignments and
cancels unfinished named research or specialist work while retaining its earned
history. An operations manager's appointment ends too. Inherited staff have
individual salaries allocated from their original payroll without changing the
opening total. Protected cash and other staffing commitments can block departure.
Former contacts retain a dated payment record and remain available in Messages.
Content **0.49.0-dev.1**, generation **50**, records the policy.
Focused checks pass for dismissal, recruiting, creative teams and gatherings,
including manager departure, protected cash, historical work and exact reload.
The older `runtime.departments` test still stops at its obsolete founding
fixture's missing opening terms before it reaches its management assertions.
That fixture needs updating separately; it is not evidence for this batch.
The bundled Release software walkthrough covers the scrolling review,
cancellation, confirmation, visible departure record, returned laptop and fresh
reload at 650 × 480 with 200% text.

Continuing Kestrel and Harbor conversations now offer follow-up funding rounds.
The recorded pitch uses founding prestige and actual published research to
select authored terms. Discussions support holding, declining, expiry and a
reviewed counteroffer sent to the actual board. Approval contributes company
capital, conserves ownership through dilution and adds the agreed seat. The
conversation reports that new seat while preserving the original opening
outcome. Generic financing controls cannot bypass a required conversation.
Nonprofit formation removes the equity-only follow-up rules with its equity
offers. Content **0.50.0-dev.1**, generation **51**, records the terms and gates.
Focused checks cover unknown contacts and unavailable devices, reputation and
publication gates, no payment on a pitch or submission, held/declined/expired
terms, conserving funded ownership, board seats, nonprofit setup and exact
reload. Governance and employee-departure regressions also pass.
The bundled Release software walkthrough covers the actual contact, offered
alternatives, holding, review cancellation, scrolling terms, an actual board
vote, visible pending/funded replies and fresh reload at 650 × 480 with 200%
text. Reply focus waits for the saved result and places its heading at the top
of the reading area.
The existing recruiting, messaging, laptop-issuance and dismissal software
walkthrough also passes with the shared conversation and scrolling changes.

Nonprofit donor follow-ups now use the same saved discussions and actual trustee
votes. Open Field offers an early institutional gift with a trustee seat; Careful
Futures requires published research and completed independent evaluation work.
Gifts have a distinct contribution journal, leave holdings unchanged and create
no sales revenue, operating profit or personal windfall. Private equity issues
and option-pool expansion are refused for nonprofit institutions. Corporation
setup removes gift programs; nonprofit setup retains gifts and removes equity
rounds. Content **0.51.0-dev.1**, generation **52**, records these programs.
Focused checks cover both gifts, trustee rights, exact reload, duplicate and
forged contributions, legal-form restrictions, and real research/training/tests
that distinguish independent evidence from started or self-reported work.
Bundled Release software walkthroughs pass for donor gifts and the existing
investor route, including holding, scrolling review, cancellation, real trustee
or board approval, visible funded replies and exact reload at 650 × 480 with
200% text. The user requested a pause after this batch; remaining F-05 work
stays in the backlog.

Retention, audience modifiers, full personal-world simulation, research balance,
expanded private-life purchases and richer event preferences remain unfinished,
as itemized in the plan and backlog.

The 2026-11-09 review rejects the main interface as complete. The detailed
[selected refinements](FEEDBACK_REFINEMENTS.md) and F-04 milestone queue in the
[backlog](BACKLOG.md) supersede conflicting setup/UI assumptions below.

F-04A implements structured gender/pronouns, a live appearance editor and founder
profiles showing their actual leg up. Wealth is explicit. A self-funded founder
starts alone at their parents' house with $0 rent; only requested and accepted
investor/donor offers contribute funds. Cofounders have agreed founding terms;
a hired CEO inherits an existing office, three employees and company cash with
a disclosed minority grant. Setup no longer chooses the first experiment, forces
a first hire, shops for offices or asks for difficulty. A home opening has no compute. Inherited office output comes from its disclosed
physical desktop computers. The budget discloses capital sources,
ownership, signing costs, rent, payroll, salary and existing/inherited equipment.
Escape during play opens the pause menu; Escape from that menu opens exit confirmation.

F-04B replaces the decorative HQ with the actual garage or inherited office.
Suppliers sell desks, computers, racks and specialist benches with paid delivery;
players place/move them, mount computers and assign people to desks. A working
desktop supplies 4 CU/day, with metered electricity; storage, delivery and power
state affect its actual output. Room functions follow installed equipment.
Realtors offer named landlords, city prices, floor plans, deposits and lease terms.
Land purchase, custom room partitions and three contractor bids lead to actual
construction, disclosed delay/overrun risks, contingency refunds and property tax.
Moving changes the actual HQ and stores its equipment for rearrangement.
Focused runtime checks cover empty/inherited offices, costs, refused placements,
forced construction risks and fresh reloads. Native Metal and software walkthroughs
exercise purchases, assignment, pointer dragging, leases, building and relocation.
The software view also checks furnishing and room placement at 650×480 with
200% text.

F-04C makes each applicant a separate compensation discussion. The player can
compare profiles, propose a salary and signing payment, revise an offer, agree
to a dated counteroffer or decline. Pay and desk assignment start with the
signed hire. The employee becomes available after paid onboarding. A focused
simulation test checks refused hires, actual payments, expiry and fresh saves.
The rendered Mac walkthrough signs a hire into a delivered desk and checks the
discussion at 650×480 with 200% text.

F-04D adds the interactive technology tree with required and alternative
prerequisites. Select a topic to review its equipment, assign named researchers
with powered workstations and reserve compute from a real source. Staff-days
and experiment CU settle separately. Projects pause with an explanation when
their resources disappear. Specialised topics require supplier equipment in
one installed lab. Topics have no start fee. Salaries, hardware, electricity
and server rentals remain actual costs. Focused tests exercise both completion
orders, power loss, prerequisites, rented compute and fresh saves. Rendered
checks cover the tree and its controls on native Metal and the software view.

F-04E adds a laptop desktop and device-specific app launcher. Players can switch
apps, move to a compatible phone app and return to the workspace. The game uses
styled buttons, inputs, selectors and dialogs. The State atlas shows the selected
state boundary, surrounding geography and interactive county briefings.
Focused Mac walkthroughs cover the device and map controls at normal size and
650×480 with 200% text. The five F-04 milestones are implemented. The broader
interface still awaits human playtesting.

All current builds, including optimized Release builds for human testers, include
**DEV · Reset local data**, which clears game-local saves, cached rules, drafts
and settings and returns to New Game. It works from a broken-save menu. Remove
its UI and implementation before shipping v1.0. Development saves have no
compatibility guarantee; verify only fresh current-build save/reload behavior.

## Selected feedback: Mac application and main menu

The first human review identified missing Mac application packaging, a
confusing game interface and an unusable Content Studio entry point. The
selected first milestone supplies **Project Glowing Pancake.app**, its icon,
bundled resources/Qt, and a separate full-screen main menu: New Game, Load
Game, Settings, Credits and Quit. Settings saves display mode, window size,
audio, text size, reduced motion and still/video background choices locally.
Escape opens an exit confirmation with Cancel and Exit game, including during
play. The Main Menu button pauses; Resume returns to the same session.

New Game now uses the founder setup described below.
The gameplay interface foundation is described below; usable Mac Content Studio remains open.

## Selected feedback: inhabitable company and portraits

The game now opens a continuing company automatically, with an original 3D
workspace, permanent HUD, device applications and HQ/city/state/country/world
navigation. Escape pauses into the menu. Accepted actions and completed days
persist atomically; storage failure stops play and preserves recovery files.
Unfinished named form fields and the current navigation are stored locally.

HQ recruitment supplies stable generated profiles, real signing costs/payroll,
onboarding and usable evaluation expertise. Branch operators remain aggregated.
The founder campaign adds 3,143 counties with a 2010 Census population/location
baseline and explicitly fictional bounded monthly economic trends. World outlines
and region briefings are offline. Employee portraits combine versioned vector
part/color IDs saved in each profile; selecting a portrait opens its dossier.

Focused core/persistence checks and native navigation/restart checks pass.
Portrait layer rendering, 1080p/8K offscreen viewports and a narrow 200% text
profile are checked separately from attached-display native Metal evidence.
Paid HQ rooms reserve real workstation capacity and open after construction.
Reusable facility blueprints buy state-specific physical sites, hardware and
operator teams through one transaction; inspection and commissioning remain
required. Owned phones, laptops, tablets and HQ workstations now have real purchase
costs, delivery, service, power and retirement history. App routing uses working
compatible equipment, including during travel; power recovery remains available
while a decision is pending. National contacts now follow the founding background
or recorded staff/revenue/publication milestones. Seven regional liaison office
offers have real setup costs, local payroll and opening delays; the first actual
overseas opening establishes world contacts. Existing public information and
obligation responses remain available. HQ operations managers now reserve one real
worker and daily compute; their engineering skill and completed management work
improve aggregate branch availability within physical limits. People shows the
tradeoff and preserves the selected page and manager/source drafts. Founder
retirement now requires a public, funded successor handover; authored messages
affect public confidence, and the saved ending enables New Game while retaining
the company and its optional sandbox continuation. Decisions now show explicitly
authored participants with profile inspection, including saved company portraits,
three named story contacts and organization cards. City labels cover all 50
states and DC from the offline 2010 Census baseline. The [selected design](GAMEPLAY_INTERFACE.md)
records the completed interface/save audit. Unfinished choices, checked sets,
selected records and ordered queues now persist with text fields, without
issuing commands on restore. New companies use their own defaults; compact
resizing keeps the focused field visible. Original content is now
retained locally so compatible content updates resume existing companies under
their original definitions. Missing collections have a reviewed recovery path
that preserves company/journal files and damaged cache bytes. Focused failure,
replay and native rendered recovery/restart checks pass. These checks
do not close human review, extended campaign or release/device gates.
The final map audit adds scaled geometry/markers and readable, click-through
hover cards at 1080p, 8K and narrow high-text layouts. Map briefings no longer
inherit another app's filter. The complete active offscreen world/pause path
passes; native rendering and native OS shortcut acceptance remain distinct.
Settings now keeps **Apply & save** and its result outside the scrolling form.
A persistent, contrasting scrollbar remains visible in full-screen mode and
compact windows. Focused checks cover applying preferences, their exact local
file, and the pinned controls at 650×480 with 200% text.

## Selected feedback: campaign and technology design

F-02 documents a new campaign direction: a California birth in 1997, formative
choices and four leg ups, founding at 18 in 2015, company/industry stages,
mission-linked objectives, distinct investor/donor/nonprofit terms, and an
age-118 ending in 2115. The completed history/result stays frozen if the player
continues in a post-game sandbox.

Both supplied technology PDFs were reviewed as advisory sources. The new plan
separates knowledge, implementation, rights and evidence; covers research
variants, publication/prestige, licensing and partnerships; and provides 140
candidate entries across ten domains plus ten bounded refinement families.
Unfounded safety guarantees, runaway compounding and a mandatory cosmic climax
were rejected or deferred. [Campaign flow](CAMPAIGN_FLOW.md) ·
[Technology](TECHNOLOGY.md) · [Candidate catalog](TECHNOLOGY_CATALOG.md).

New Game opens on the hospital prologue. Each beat uses original silhouette art,
staged narration and one decision on a proportional canvas. Family background
and legal form change later illustrations and accents. Identity, traits, appearance and eight
formative choices lead into entry route, legal form, mission and explicitly
requested funding terms. The signing review reconciles
contributions, costs, monthly commitments and ownership. The complete biography
persists in campaign saves. The company starts paused in 2015.

Standalone sandbox creation was removed. The save reader still supports prior
saves. Later stages, ending presentation and the first-play tutorial remain
in the backlog. [Images and pacing](../assets/founding/README.md).

## Implemented core

| Area | Available starting point |
| --- | --- |
| Campaign and persistence | Company setup and seed choices, paused calendar, local saves, autosaves, exact continuation and a playable 2014–2017 campaign |
| Lab | Licensed, commissioned, feedback and synthetic data, research and setbacks, training and checkpoints, independent evaluation, deployment and specialist assignment |
| Business and people | Accounts, contracts, forecasts, taxes, funding, protected budgets, paid hiring and onboarding, staffing pressure, executives and board decisions |
| Products and competitors | Product channels, staged releases and rollback, compatibility and retirement, enrollment, incident response, rival plans and heavyweight responses |
| Geography and infrastructure | Domestic map, international regions, owned and leased compute, power, delivery, resilience, insurance and wholesale contracts |
| Society and long campaigns | Civic policy, elections, regional response, institutional cases, conditional research, succession, objectives, postmortems and sandbox continuation |
| Content and authoring | Shared event runtime, built-in news and optional stories, separate Content Studio, typed graph/forms, inspection and versioned content snapshots |

The [playability guide](PLAYABILITY.md) describes controls and playable paths.
The [implementation log](IMPLEMENTATION_LOG.md) records the behavior and
focused engineering checks.

## Current development build

Plain `./make.sh` builds both applications in Release with local CPU tuning,
`BUILD_TESTING=OFF` and developer diagnostics disabled. It runs no tests and
resets a cache previously configured with tests. `--enable-tests` explicitly
includes test targets/GUI checks; `--test` also runs tests. `--enable-debugging`
selects Debug, and `--portable` disables host CPU tuning. Incremental caches
are retained. The Codex Run action builds/packages/launches only the game via
`script/build_and_run.sh`; builds alone never launch an application.

F-04A focused runtime/bridge checks pass for founding routes, structured identity,
portrait editing/profile display, zero-capacity Lab defaults and current-build
save/reload. The reset check recovers from an unreadable save, deletes only
application-local data (without following links outside it) and starts a fresh
company. Offscreen rendered prologue walkthroughs pass at 100% and 200% text,
including the 650×480 canvas, live appearance editing, funding, paused office,
first day, fresh export and automatic day continuation. The menu walkthrough
passes one-click reset, settings, both Escape paths, cancellation and actual exit.
These are focused Mac development checks using the software renderer, not new
native Metal, OS shortcut, reader or clean-device qualification.

No full suite, long campaign or native matrix is part of this batch. Native
file-picker row selection, VoiceOver and release/device qualification remain
open. The prior forecast-padding and full-campaign staffing assertions remain
recorded for later selected work. Content Studio usability is not accepted by
this milestone.

Finalized content is generation 52. Game and editor versions remain development
placeholders, and release readiness is false.

## Next phase

1. Gather and review human feedback and the user's work-in-progress notes.
2. Select concrete changes and refine the core around that feedback.
3. Expand events after those changes establish the direction.
4. Balance and undertake extended qualification afterward.

`tech_sug.txt` remains an unchanged work-in-progress reference. The selected
technology design above records what is now adopted as planning direction;
other ideas still need review/selection before implementation.

The [backlog](BACKLOG.md) carries the deferred human, QA, packaging and
publication gates with their original IDs. Their scheduling follows the
user's next phase instructions.

## Initial feature coverage

The original 52-step tracker is archived at **42 recorded implementation
acceptances and ten deferred later gates**. Its unchecked items retain their
original criteria. Closing the skeleton phase records the user's handoff
decision.

- [Original checklist](archive/initial-implementation-backlog.md)
- [Implementation coverage and handoffs](archive/initial-implementation-status.md)
- [Original milestone roadmap](archive/initial-roadmap.md)
