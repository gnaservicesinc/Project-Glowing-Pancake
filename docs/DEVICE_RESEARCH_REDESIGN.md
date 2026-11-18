# Devices, inherited companies and an unfinished research frontier

Selected design direction · 2026-11-18 · Implementation plan, not completed features

This records the user's current playtest feedback and their clarification that
the technology tree cannot be finished and is not meant to be finished. It
supersedes conflicting interface, founding, data and technology assumptions in
the older plans. The two screenshots are evidence of the current experience;
their displayed text is not a separate source of instructions.

## The experience being built

Lead an institution with a history, people, commitments and a technical identity.
Use convincing applications on the protagonist's actual computers and portable
devices to understand that institution and make decisions. Research choices
change what the lab can build. Data has identifiable origins and a production
history. Time, money, talent and competing opportunities prevent any one company
from exhausting the research frontier.

The early struggle is to produce credible work and gain recognition while
paying the bills. Prestige can attract funding and talent; buzz can attract
attention and speculative investment. Wealth can finance work without either.
A small or solo lab can pursue a narrow direction while burning its limited
cash. As useful technology becomes popular, staying relevant and financing the
next improvement become harder. Products, contracts, licenses or sustained
institutional funding can support continued work. These are interacting
strategies, not a mandatory checklist or a fixed date at which every lab must
become a commercial platform.

## 1. Joining an existing company

“Start at Alder Research” currently selects an authored biography, Robin Ibarra,
and an appointment as CEO of a staffed existing lab. It is a normal career
opening, not God mode or a testing scenario. Rename the entry to **Join Alder
Research as CEO** and put the explanation beside the choice, rather than only
in a tooltip. State that the biography is prepared and that creating one's own
protagonist remains another route.

An appointment is employment under existing company terms. Present an
appointment letter with the lab's purpose, legal form, existing bylaws, owners,
board, delegated authority, compensation, offered equity, cash, payroll,
facilities, current work and commitments. Accept, decline or review the
agreement; do not expose a formation form that lets an applicant rewrite it.
Apply the same rule to custom protagonists accepting an existing lab's offer.

The company owns its funds and equipment. The protagonist retains their own
savings and receives their actual contractual salary/equity. A title does not
give the employee all the powers of a sole owner. The existing organization's
mission and bylaws must come from authored company state and be enforced by
commands, not merely disabled selectors.

Distinguish ordinary operating decisions from changing the institution's
purpose or rules. Fundamental changes require the appropriate controlling
ownership and voting rights. Any further charter/contract constraints remain
visible; obtaining a controlling economic stake must not silently grant votes
it does not carry. Show the concrete route to obtaining control, its price and
consequences. Reversing the inherited purpose after gaining control produces a
public scandal, with employee, donor/customer and investor reactions reflecting
their actual interests and commitments.
The scandal is survivable: disclose response options and allow continued play,
rather than treating the reversal as an automatic loss. Founding another
company remains a separate route to choosing a new purpose and new terms.

## 2. Applications that inhabit devices

Existing management screens remain available as direct tools through the menu
bar. Device icons must launch applications designed for their screens, rather
than sending the player out to those management screens. Both presentations
read the same company records and use the same validated commands. There is
one simulation, not separate app balances or parallel project systems. Direct
menu access does not bypass equipment needs, permission or command validation.

Use the computer/tablet popup as the normal application surface. Establish a
small common vocabulary: home/launcher, open-app switcher, app navigation,
search, selected-record detail, a contextual action and readable feedback.
Use adaptive master/detail layouts on computers and larger tablets; stack
list → detail → reviewed action on narrow devices. Keep navigation and the
primary action reachable while long content scrolls. Give each app a visual
identity without inventing a different control system for every app.

Graphical elements must help make decisions and be interactive: select a
research node to inspect or assign work; select a budget segment to see its
actual expenses; select a dataset's source or processing step to follow its
lineage; select a person to inspect availability and assignments. Supply a
readable list/table equivalent where it is useful. Do not substitute decorative
charts for missing operations, or hide essential actions behind hover.

Suggested first application flows, to be refined through rendered review:

| App | Opening view | Main interaction |
| --- | --- | --- |
| Research | Current projects and a spacious technology map | Compare paths, inspect a topic, form a team and review its commitments |
| Data | Corpus library with origin, usable volume and processing state | Compare sources, commission/obtain material and plan cleanup |
| Models | Experiment/checkpoint cards and evidence | Inspect lineage, compare measured results and prepare training |
| Accounts | Cash, committed costs and runway | Drill into expenses and compare a reviewed financing offer |
| People | Named people, current work and availability | Compare applicants, negotiate and assign actual staff |
| Products | Product designs, releases and customer needs | Explore feasible designs and their technical tradeoffs |
| Messages | Conversations, offers and decisions | Read a thread with its context, participants and commitments |

These are application responsibilities, not a requirement to build independent
subsystems for every row. Existing facilities, operations, world, leadership and
personal tools need the same treatment as they are brought into the app suite.

### State belongs to a physical device and an app

Persist a workspace only for devices the protagonist actually uses. Key it by
stable physical identity: personal laptop record, particular tablet, or the
specific installed HQ computer. `desktop`, `tablet` and `laptop` are kinds,
not identities. Two computers of the same kind can have different workspaces.
Do not maintain app sessions for every NPC computer.

Each used device remembers its open apps and active app. Each app instance
remembers its selected page/record, filters, search, scroll, graph viewport and
unfinished form/selection state. Suspending an app preserves it; explicitly
closing an app removes it from the open-app switcher. Switching devices resumes
that device's previous workspace. Device A can hold Research open while device
B holds Accounts, and each can switch back to its other open apps independently.

Company records are shared. If a project completes while another device shows
its detail, refresh the record while preserving that device's presentation
state. An unsent form is not an accepted command. Recheck its quote/revision
when submitted, explain stale terms and preserve the editable draft. Restoring
a session must never purchase, sign, train or consume randomness.

Power loss, delivery, service, location and ownership determine whether that
physical device can be used. Keep its workspace while unavailable. Explain
the interruption and offer another eligible device without silently treating
that other device as the interrupted one's session. Career changes must also
respect equipment ownership and access to the previous employer's records.

### Detached windows

In windowed mode, **Pop out device** moves the live device surface into a native
window. Docking or closing that window returns it to the main window and keeps
its workspace. Other used devices can have their own windows; one physical
device has one live surface. Main-window closure still owns the normal game
exit. All windows share calendar, pause, decisions and the authoritative
simulation. A second view cannot double-submit a transaction or run a second
simulation clock.

**Pop out technology tree** opens an expanded research workspace with its own
usable graph area and inspector. It remains connected to the same selected
project/company and command authority. Define whether the source surface is
moved or observed in place, and keep the selected topic synchronized when
appropriate; never create a disconnected copy of the research state. Track
window geometry and return off-screen windows to a visible display. Full-screen
play retains usable embedded device and tree views.

## 3. Research explains itself

The supplied tree screen gives too little height to the graph and puts dense
technical descriptions, qualification rules and a long trait list in one
inspector. Worker reasons overlap their controls. Redesign the information
hierarchy, not just the frame around it.

Give the graph the available work area, with pan, zoom, fit, search, branch
filters and a clear location in the wider field. Keep the inspector resizable
or collapsible. On a narrow device, switch between the graph/list and the
selected topic's details. Opening the expanded tree must use the extra space.

Explain a topic in this order:

1. What the team will learn, in plain language.
2. What that makes possible, with selectable product/model/process examples.
3. What is missing: prerequisites, people, equipment, data if applicable.
4. Work, compute, expected time and resulting ongoing costs.
5. Choose a team/source, review the plan and begin.

For example, describe reproducible tooling as keeping experiments repeatable
so the team can check results and reuse the work. Put precise requirements and
trait contributions in an expandable explanation. “Needs creativity” must say
what blocks this attempt and how the selected team affects it. A person row
needs adequate height for its reason; an unavailable worker's explanation must
not sit over another worker's button.

Stories need the same clarity. “The missing zero” concerns a passenger count:
an archive scan says 108 and a transcription says 18. Introduce Nell as the
editor asking the lab to help verify the number, explain why the lab's methods
are relevant, introduce Gus as the person holding the original ticket records,
and explain what accepting the investigation commits. Do not require the player
to infer all of that from unexplained names and a metaphor about a headline.

## 4. Data is obtained for a reason

There is no universal dataset gate for doing research. Work on an algorithm,
theory, tooling or hardware can proceed without purchasing a corpus. A project
that needs observations, benchmark material or an experiment dataset declares
that specific need. Training a model requires an appropriate chosen dataset or
mixture at the point it is planned, not at company formation or before any
research can start.

Research projects can produce proof-of-concept models as actual outcomes. Keep
their task scope, training inputs where needed, compute, authors, method,
checkpoint and measured evidence. They can support a paper, demonstration,
further experiments or subsequent development. A proof of concept does not
automatically satisfy production reliability, scale, rights, evaluation or
serving obligations. Reuse it through an explicit continuation/development
path, preserving its actual work and lineage.

### Sources and quality are separate dimensions

Every obtained corpus has an identifiable source. “Licensed” is a permission
arrangement, not a quality tier. A public dataset can have excellent material
or poor, contaminated material; redistributed public material retains its
underlying source history where known. Torrent acquisition does not itself
establish quality or permission. Scraping must identify the fictional source
and collection approach; those choices affect diversity, noise, scale,
duplication, representativeness, permission and exposure.

| Acquisition path | What the player chooses or inspects |
| --- | --- |
| Existing content the company creates | Actual content-producing activity, rights and rate of usable output |
| Torrent/distributed collection | Named collection, declared/unknown origins, usable volume and permission uncertainty |
| Licensed collection | Named supplier, sampled quality, coverage, terms, price and permitted uses |
| Direct scraping | Source/site category, collection method, rate, filtering, scope and resulting provenance |
| Public dataset | Publisher, underlying collection history, version, coverage and stated rights |
| Dedicated in-house creation | Named hired creatives, relevant skill, brief, review, payroll and slow original output |

Feedback and synthetic production already have useful underlying systems;
integrate their lineage and actual production rather than losing them in this
rework. Do not equate original human content, user feedback, simulations and
teacher-generated examples merely because all were produced internally.

Represent raw amount, usable unique amount, quality by relevant task, coverage,
duplication, contamination, provenance confidence and use rights separately.
Source characteristics establish the starting material. A processing job can
deduplicate, filter, correct, label, balance or review it. The resulting quality
and retained amount depend on the operation, staff skill, equipment/compute,
time and source limitations. Show the volume/quality tradeoff before committing
and preserve immutable processed versions and their lineage afterward. Cleanup
cannot invent missing provenance or grant missing rights.

### Dedicated creative work

Hiring creatives to produce original corpus material is a real staffing route,
with relevant creative skill, availability, compensation and actual production.
Its quality depends on those creatives' relevant skill and the work they do;
the route supports the game's highest attainable data quality. It is not an
automatic perfect-quality label attached to anything called “in-house.”

This route is slow and initially cannot supply enough material for large models.
It has no controversy from third-party collection in the game's source rules.
That makes its clear origin strategically valuable when events change which
data customers, funders or institutions will accept. Other controversies about
company conduct remain independent.

Research on smaller models, sample efficiency and related techniques can make
limited high-quality material sufficient for increasingly useful models. Unlock
those improvements through the relevant branches and measured capabilities;
do not hand every company an unlimited synthetic multiplier or make expensive
creative output instantly substitute for a huge diverse corpus.

Data provenance matters more as models, audiences, distribution and scrutiny
grow. Events should act on actual source/processing/model lineage and disclosed
rules. Preserve the ability to investigate, replace data, retrain, limit a
release or change product scope, with real time and financial consequences.

## 5. Research produces a technical identity

The tree is a frontier to navigate throughout a life, not a completion board.
It needs substantially more distinct work than a single company can pursue.
Later branches widen the opportunity set and continue requiring choices.
Do not present an overall completion percentage as a goal, converge every
strategy on one compulsory ultimate branch, or make “research everything” the
best company plan.

Use AND requirements where a capability truly combines several advances. Use
OR alternatives where different methods reach a useful next capability. Permit
groups such as `A AND (B OR C) AND (D OR E)`, validated for cycles and readable
in both graph and list. A company can advance through B and skip the entire C
branch; an apparent alternative that later requires C anyway is not a useful
strategic alternative. Preserve specialist branches and worthwhile applications
at different scales.

Research paths affect feasible product configurations and traits: quality on
particular tasks, data efficiency, compute/memory needs, latency, robustness,
modality, deployment setting and integration. Show those concrete possibilities
when comparing paths. Combine compatible discoveries and implementations;
research emphasis must have consequences beyond a generic percentage bonus.

Long projects should compete with obtaining an available implementation or
license. Compare price, integration time, contractual scope, dependencies and
renewal against internal research effort, salaries, compute and delay. A
license can provide valid use of a required capability where its terms allow;
it need not award original discovery credit or disclose the supplier's entire
method. Public knowledge still needs implementation and evidence. Expiry must
apply its disclosed restrictions without erasing historical knowledge.

Expand content by technically meaningful families and alternative routes, not
by renaming a chain of identical upgrades. Frontier/refinement work can remain
available with diminishing practical returns; it must not create unbounded
daily compounding or a mandatory speculative physics climax. The campaign must
leave desirable unexplored directions for specialists and rivals. A finite
authored catalog alone is not evidence of this: review attainable work budgets,
branching opportunities and late-game continuation against the no-completion
design requirement.

## Current implementation and the work it requires

The current code already provides validated commands, local persistence,
devices with power/service, paid project allocations, AND/OR research links,
dataset mixtures, commissioning, feedback, synthetic production and model
lineage. Preserve those foundations.

The important gaps observed in source are:

- The prepared appointment is labeled as an unexplained start shortcut.
  `HomeStart.qml` still offers a mission selector for a hired CEO; the bridge
  treats mission as editable unless a cofounder sets it. Existing bylaws are
  not presented as a complete inherited appointment agreement.
- `DeviceHome.qml` routes icons into the shared surface in `Main.qml`.
  `WorldController::devices()` collapses available equipment by kind, and
  `resolve_app_device()` returns a kind. Navigation holds one app/device, while
  research drafts use shared keys. These contracts cannot represent independent
  workspaces on two actual computers.
- `ResearchTree.qml` reserves a short graph viewport inside a larger scrolling
  form. The inspector contains dense requirements, team/trait text and workers.
- Data has provenance, rights and several production paths, but the dedicated
  creative workforce, source/processing workflow and the requested connection
  to research-produced proof-of-concept models need explicit extensions.
- The reviewed 140-candidate catalog is a proposal, not proof that the shipped
  tree has that breadth or the intended late-game alternatives.

## Implementation order and acceptance

These are cohesive milestones. None is complete merely because the existing
screens remain reachable or an engineering scenario passes.

| Milestone | Result to build | Acceptance path |
| --- | --- | --- |
| F-08A · Appointment and authority | Clear career entry, inherited institutional terms, enforced control rules and survivable reversal consequences | Accept a prepared and a custom-character appointment; inspect existing terms; refuse an unauthorized rewrite without mutation; obtain actual control and enact a consequential reversal; found a separate company with chosen terms |
| F-08B · Physical device workspaces | Stable device identities, independent app sessions, menu-bar access to direct management tools and detached native device windows | Open Research on one computer and Accounts on another; switch apps independently; save/reload unfinished work; power off one device; resume its workspace after recovery; dock/undock without lost work or duplicate commands |
| F-08C · Usable research app | App-specific project/graph/inspector flow, expanded tree window and plain-language explanations | Find a topic, compare paths, inspect missing requirements, choose qualified workers and start real funded work; use compact/high-text layouts with no overlapping worker controls; preserve graph position through switching and reload |
| F-08D · Source and processing simulation | Named acquisitions, processing lineage, creative staffing and data-dependent research/PoC outcomes | Research a data-free topic without a corpus; supply data only to a project that needs it; obtain and clean a source; hire creatives and observe paid slow output/skill effects; produce, inspect and reload a real PoC model |
| F-08E · Broad research and product choices | Expanded validated alternatives/combinations, licensing tradeoffs, small-model efficiency and technical product identity | Reach the same useful capability through different branches while skipping alternatives; compare a lengthy internal project with an actual license; build meaningfully different products; demonstrate remaining worthwhile frontier choices late in a feasible campaign |
| F-08F · App suite and changing industry | Remaining app-specific workflows and staged funding/data/product consequences | Run early research/prestige, wealth-funded and small-lab routes; see an industry boom alter opportunities; sustain the lab under real costs; inspect and respond to an event grounded in actual dataset lineage |

Define the shared record/command contracts before their consumers. A practical
dependency order is F-08A, then device identity/session and application shell in
F-08B, then F-08C. F-08D establishes source/processing and PoC contracts before
F-08E content depends on them. F-08F completes the app suite and integrates the
industry progression. Do not declare the overall redesign implemented at the
end of its first milestone.

For each implementation batch, use one retained optimized Mac build and focused
checks for actual command effects, resource costs, failure/recovery and fresh
save/reload. Inspect rendered normal and compact/high-text interfaces, including
detached-window focus and closure. Human usability acceptance remains distinct
from those checks. No release matrix, prior-development-save migration or
external service is required by this design pass.
