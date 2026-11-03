# Project Glowing Pancake — final game design

Design baseline 1.2 · 2026-11-03 · Working title · Implementation has not started

This is the product design for the full 1.0 game. Numeric balance targets are starting values to test, not claims about the real world. [Decisions and requirements](DECISIONS_AND_REQUIREMENTS.md) explains the relationship to the two drafts. [The roadmap](ROADMAP.md) separates the complete design from the first playable release.

## 1. The game in one paragraph

You found an AI company in 2014 and guide the institution through a century of technical, commercial, and political change. Design models, acquire data and compute, recruit people, find customers, build infrastructure, negotiate power, and live with what your products change. A small specialist business, an open research institution, a profitable platform, and an ambitious frontier lab are all credible strategies. The player learns through decisions, forecasts, experiments, and consequences. The game never requires real AI training, an online account, or a connection to a model provider.

**Genre:** single-player, pauseable business and political strategy simulation. **View:** clickable first-person location sets with management overlays and maps. **Audience:** players of management and grand-strategy games; no machine-learning knowledge required. **Price/license:** free; GPLv3 code, with separately credited compatible asset licenses. **Primary release:** downloadable desktop game on itch.io.

## 2. Design pillars and boundaries

1. **Make a company, not a prescribed build.** Combine research, operations, products, organization, and influence. There is no mandatory sequence of scripted CEO decisions.
2. **Make causes visible.** Every important result has a readable explanation, every budget has units, and major risks have warning signals. Forecasts disclose their uncertainty.
3. **Make ethics consequential and contextual.** Privacy, access, safety, openness, concentration, and labor effects interact with markets and institutions. There is no universal morality meter and no single ideological correct answer.
4. **Keep complexity manageable.** Rich strategic decisions, delegated routine work, clear comparisons, and pause at any time. Geography and politics must change decisions rather than merely add tables.
5. **Let the future branch.** AGI, ASI, automation, energy abundance, and stagnation are scenario possibilities. Dates after the authored historical reference period are not predictions.
6. **Respect the player's computer and privacy.** Complete offline play; no collection of player information, telemetry, advertising, or background communication. Local preferences, saves, autosaves, and campaign history are allowed.

No multiplayer, real model execution, external generative dialogue service, 3D walking, tactical combat, individual citizen simulation, paid currency, or live financial data in 1.0. State-level geography ships first; county detail is a later expansion. Room sets supply place and personality without requiring character animation or travel chores.

## 3. Campaign setup, time, and continuity

### Default campaign

The company starts in the United States on **2014-01-01**. The campaign simulates the interval up to **2114-01-01**, when final objectives are evaluated. The end date therefore represents exactly 100 years; the last playable day is 2113-12-31. Endless sandbox removes the objective deadline and supports continuing the same institution.

Setup selects a company name, founder name/pronouns, headquarters state, one founder background, an organizational charter, difficulty, world seed, and primary objective. All names can be generated locally. No real player identity is requested. A preview explains each starting modifier.

| Background | Opening advantage | Opening limitation |
| --- | --- | --- |
| Researcher | Research execution and academic contacts | Fewer commercial contacts |
| Product builder | Customer discovery and deployment speed | Smaller research network |
| Infrastructure operator | Procurement and uptime | Less initial model expertise |
| Community organizer | Hiring network and partnerships | Smaller initial funding offer |

Backgrounds change the opening, not permanent ceilings. The charter sets initial board priorities and employee expectations; it can be amended through the board. Difficulty changes starting runway, rival competence, financing terms, and forecast precision; it never secretly changes outcome rules or makes opponents omniscient.

### Pace and delegation

The authoritative simulation advances in whole days. Player controls are pause, normal, fast, fastest, advance one day, and advance to the next chosen milestone. Normal is initially one simulated day per real second; faster speeds are maximum rates, not guarantees. The UI stays responsive while the simulation catches up.

Training completions, financing deadlines, severe incidents, insolvency warnings, board votes, and new legal obligations can auto-pause. Players configure other alerts. Delegation policies can renew contracts within a ceiling, maintain cash reserves, replace obsolete hardware, schedule evaluations, and adjust prices within a range. Every policy has limits and a decision log; it cannot spend without its budget or change the company's charter.

The normal pause point is the morning briefing: today's deliveries, effective rules and due events are visible before assigning work. Required opening choices resolve before work can proceed. Separate required decisions/alerts, actionable inbox items with their actual simulated deadlines, and background news grouped in a digest. Routine bulletins do not repeatedly stop fast-forward. No choice requires a real-time reaction.

Later delegation adds a short bounded research queue/standing track policy and mature-product refresh under cash, staff and spare-compute ceilings. New queue items recheck prerequisites and budgets. Refresh creates and evaluates a candidate within licensed rights before an authorized rollout; it cannot skip evaluation or displace protected serving capacity. Players can review, suspend and override each policy.

The first 3-year scenario should take 30–45 minutes. A full campaign targets 12–20 hours with delegation and fast-forward. These are playtest targets. When an early year and a mature year demand the same repeated work, improve delegation before adding content.

### A century without an immortal CEO

The player guides the founding institution. Founder succession is introduced before it becomes relevant: appoint a successor, move to the chair, and establish governance rules. Retirement changes relationships, public persona, compensation, and board leverage; it does not reset the company or end the run. Succession events offer planning time. The interface distinguishes founder legacy wealth from the current executive's salary. No age-related death descriptions are needed.

## 4. The loop and the first session

**Observe → make a plan → allocate resources → develop/test → deploy → see consequences → adapt.** A decision often creates opportunities in another system: publishing attracts applicants but also speeds competitors; a cheap compute deal consumes scarce cash or gives a supplier board influence; a reliable small model can beat a prestigious large model on a customer's actual task.

The first 30 minutes introduce the loop through work:

1. A prospective customer asks for document classification, showing task requirements and an affordable contract.
2. Compare two data packages, each with quality, coverage, provenance, rights, and cost information.
3. Hire one missing specialist and lease a small compute allocation.
4. Configure a feasible training project. The preview shows time, expected results, operating cost, and the remaining runway.
5. Evaluate on a separate test set; examine an attractive headline score that hides a weak customer subgroup.
6. Improve the data, reduce the promised scope, or deploy with an explicitly accepted limitation.
7. Negotiate the contract and see actual usage, margin, customer feedback, and staff reaction.
8. Choose whether to publish a method, retain an advantage, or offer a limited partnership.

The tutorial can be skipped and its guidance reopened. It uses the same mechanics as the campaign. Tooltips introduce concepts at the moment they affect a decision; an optional reference desk offers more depth and sources.

## 5. Company, products, and finance

### Customers and distribution

Demand is divided into enterprise, developers, consumers, public services, science/education, and industrial operators. Each segment exists in each world region and has a changing budget, task mix, willingness to adopt, and requirements for price, capability, reliability, data handling, and vendor trust.

Products select a model/version, market, permitted use, support level, serving capacity, distribution channel, and pricing. Channels unlock through research and organizational competence:

| Channel | Main opportunity | Main burden |
| --- | --- | --- |
| B2B contracts | Predictable recurring revenue | Delivery requirements, support, procurement delays |
| Developer API | Broad integration and efficient distribution | Metered serving cost, uptime, abuse controls |
| Consumer application | Reach and brand | Acquisition cost, churn, moderation, inference demand |
| Licensed/on-premise model | Buyers who need local control | Sales cycle, support, slower upgrades |
| Open-weight distribution | Adoption and ecosystem growth | Limited direct capture, irreversible weight release |
| Agent service | More valuable completed work | Tool permissions, oversight, reliability, liability |

Open weights are not automatically open-source software; the release policy names exactly what is shared and under what terms. Customers can use competing vendors. Useful work delivered, rather than downloaded weights or advertised parameters, determines market share. Free products consume serving capacity and cash. A waitlist protects reliability but limits growth.

Products have versions, staged rollout, rollback, deprecation notice, and compatibility commitments. A service can earn money from a mature model while a frontier project loses money. The player may remain a specialist throughout the campaign.

The initial twelve template families are document classification and industrial visual inspection (B2B); coding assistance and retrieval/knowledge services (API); a general assistant and non-sexual social/companion engagement (consumer); a private organizational model and an edge-perception model (licensed/local); a research toolkit and an education model (open weights); business-workflow automation and industrial/robotic coordination (agents). Domain configurations add finance, health/scientific research, public services, lawful security analysis, and other customer tasks with their own evidence and permission requirements. These are starting configurations, not twelve rigid character classes: players may combine feasible components and target underserved tasks within the simulation's data/recipe constraints.

An infrastructure provider can sell reserved owned-site compute through wholesale capacity contracts from M5, without first training a commercial model. Price, term, compatibility, uptime, energy inclusion and curtailment obligations matter; supplying a rival earns revenue but can accelerate its research. This is infrastructure revenue alongside the six model channels. Hosting does not grant duplicate delivered-AI market share, and a sold capacity slice cannot also run the seller's projects.

### Financial simulation

Track cash, receivables, payables, payroll, recurring costs, capital expenditure, depreciation, debt, taxes, equity, and dividends. Show cash flow, profit/loss, balance sheet, unit economics, and 3/12-month scenarios. Forecasts include committed spending and a clearly labeled demand range. Insolvency is about liquidity and obligations, not a negative accounting profit alone.

All monetary amounts use constant-2014 purchasing power, with a visible unit label; the default game does not compound nominal inflation over centuries. Real cost and financing changes still affect decisions through their explicit rules. Content thresholds, contracts and profit comparisons therefore keep the same money basis.

Financing choices are founder capital, customer prepayment, grants, equity rounds, debt, strategic supplier agreements, and a later IPO. Equity dilutes a conserved cap table; rights and board seats are separate terms. Grants constrain eligible spending. Cloud credits expire and are not cash. Debt has covenants, interest, repayment dates, and refinancing risk. Going public changes disclosure and board pressure; it does not create limitless funding.

Negotiations use offers and counteroffers over price, duration, service levels, rights, governance, and exclusivity. Counterparties have interests, alternatives, patience, and budgets. No collectible cards or agent-card resources gate negotiation. Repeated offers do not reroll the same deal.

## 6. The ML lab and research

### Model design

The model is a designed system, not one intelligence bar. Its profile reports language, perception, coding/reasoning, scientific/industrial use, social interaction, and autonomous execution. Report cost, latency, reliability, robustness, and control/oversight separately. A model with strong benchmark results can still be commercially unsuitable or difficult to supervise.

A project chooses architecture family, scale class, training recipe, dataset mixture, allocated compute, target capabilities, evaluation plan, and deployment controls. The preview flags memory/interconnect incompatibility, bottlenecks, uncertain data quality, likely training time, and serving cost. You may cancel, pause, resume, or checkpoint; consumed compute and incurred non-refundable payments stay spent. Reversible unconsumed allocations release their reservations, while immediately delivered licenses and confirmed obligations follow their disclosed terms. Preview irreversible costs before confirmation. Existing deployments continue to need capacity, with a visible compute-per-task requirement.

### Data

Sources include public-domain/openly licensed corpora, negotiated licenses, academic partnerships, commissioned/consented work, opted-in product feedback, acquired datasets, and synthetic data. A fictional disputed-provenance offer can be a business dilemma; the game provides no real instructions for unlawful collection. Dataset rights in the fictional simulation do not license any actual bundled game asset.

Each dataset exposes volume, domain fit, language/region coverage, deduplication, contamination estimate, quality confidence, consent/provenance, use rights, price, and renewal terms. These attributes are independent: expensive data can be poor, and public data can be excellent. More synthetic data is useful in suitable workflows but cannot create verified information from nothing. A bad data mixture can erase specialist performance.

### Research and evaluation

Research is a graph with alternative prerequisites, complementarity, and diminishing returns. Tracks cover architectures, optimization, data methods, evaluation/control, inference systems, hardware/infrastructure, and product tooling. Discoveries create options rather than flat global percentage buffs. Research projects use staff, time, money, and sometimes experiments. Failure gives partial evidence and reusable knowledge, with an explanation.

Publishing a paper, releasing code, sharing a dataset, releasing weights, selling a license, and keeping a trade secret are distinct decisions. Benefits depend on audience, evidence, license, and actual usefulness. Publicity alone does not automatically raise trust. Rivals adopt public work after time and adaptation cost. A public release cannot be un-released by changing a policy later.

Evaluation requires time, staff, and held-out evidence. Show known performance, coverage gaps, and uncertainty. The same displayed score can support different deployment judgments depending on test coverage. Test-set contamination boosts the misleading score but not real customer performance. Independent audits, interpretability, access controls, staged rollout, monitoring, and incident response reduce different risks; none is a universal safety switch.

Findings describe concrete task/subgroup limitations, such as unreliable scanned tables, with their evidence and coverage. Suitable tests or field experience can discover a previously unknown failure; tests do not reveal all hidden flaws. Targeted data/retraining, a matching deployment control, or a narrower customer promise are alternative responses with real costs and fresh evaluation. A label cannot itself cure a model.

**Safety is not a fixed capability penalty.** Some controls cost latency or money, some improve reliability and sales, and some fail under distribution shift. Each intervention states its modeled mechanism and limits. The mathematical abstractions are defined in [the simulation specification](SIMULATION_SPEC.md).

## 7. People, executives, the board, and public identity

Staff are teams with capacity, specialties, salary, experience, morale, burnout, and institutional knowledge. Individuals are simulated only for major executives and a small number of key people. The player sets team assignments, leadership, compensation, hiring targets, working conditions, and promotion policies.

From M4, negotiate cash-only or cash-plus-equity compensation from the reserved option pool, with visible vesting/departure terms. The pool is part of existing fully diluted ownership; grants cannot create a second copy of it. Enlarging it needs the required board approval and shown dilution. Equity may help retention/runway but cannot replace agreed cash payroll for free. The slice reserves its initial pool without a detailed option market.

Named leaders have technical/commercial competence, values, risk tolerance, personal commitments, relationships, and promised terms. Values are overlapping priorities, not political caricatures. Someone can favor both rapid open research and careful deployment. A disagreement escalates through private concern, a documented recommendation, a deadline, and a possible departure or public disclosure. No important resignation or board removal occurs without readable causes and response opportunities, except a clearly labeled severe external emergency.

The board reviews liquidity, strategy, delivery, charter obligations, and governance. Voting follows seats, rights, and positions. Founder control is neither permanent nor a magical immunity. Losing the CEO position can continue the game as chair if governance allows; complete loss of institutional control ends the campaign with an explanation and an option to continue an unscored observer sandbox.

Public identity is an audience-specific record of claims and actions. Investors, researchers, customers, staff, regulators, and communities can disagree about the founder. Broken promises, credible evidence, and previous outcomes matter. Persona labels are descriptive summaries, never an irreversible class or morality score.

## 8. Infrastructure and geography

The domestic map covers **50 US states and DC**, with a list/table alternative. State-level build sites have capacity, power price, grid mix, connection delay, land/building cost, cooling requirement, talent access, connectivity, taxes/incentives, water constraints, and regional hazard exposure. Values are authored, sourced where useful, and labeled as game approximations. A state does not imply identical conditions at every location.

The player can lease cloud compute, colocate equipment, or build an owned site. Hardware has delivery lead time, purchase/lease terms, useful life, memory class, interconnect, power draw, maintenance, and resale/scrap value. Training, inference, and research share a finite capacity pool. Buying equipment does not grant immediate grid access or qualified operators.

Compute uses one fixed scenario reference across eras. Hardware/recipe compatibility and bounded authored efficiency improvements drive upgrades; a new era does not silently redefine CU or make every old machine obsolete. Older compatible models can remain useful on legacy equipment. The exact progression is a balance result, not a promised real-world hardware-growth curve.

Redundancy, backups, geographic replication, insurance, maintenance, and contracts mitigate specific hazards at a cost. Fire, storms, floods, earthquakes, heat, and grid outages are abstract disruptions to people availability, equipment, service, and finance. Staff welfare is acknowledged without injury descriptions. Preparedness changes impact; choosing a state never creates a guaranteed disaster script.

The international map has eight market regions: US; Canada; Europe (EU and UK aggregated); China; India; East/Southeast Asia and Oceania; Latin America/Caribbean; Africa/Middle East. This is an explicit simplification, not a claim of political unity. Each has country-level profiles where needed for a contract, rival, trade restriction, or event. Foreign operations use aggregated site profiles; they do not simulate every province. A region may split into successor profiles in a scenario, with a documented migration of population, demand, assets, and obligations.

## 9. Competitors and the world

Default world generation schedules six domestic startup rivals, six international rivals, and three heavyweights. Three domestic and three international startups are active initially; the remainder enter as the simulation develops. Settings can reduce or expand the roster within tested limits. Names and leaders are fictional; locations do not dictate personality or ideology.

Rivals use the same budgets, delivery times, capacity limits, and market rules. Their strategic profiles differ in horizon, openness, specialization, governance, and appetite for risk. They research, sell, hire, publish, partner, consolidate, and sometimes fail. They see public releases and market intelligence, not the player's undisclosed settings. Intelligence is a paid estimate with uncertainty, not espionage minigame cards.

Heavyweight response has four states: watch, mobilize, compete, and consolidate. Growing substitution risk to an incumbent's businesses, demonstrated adoption, rival financing, and sustained attention create pressure. **Code Red is a response to market evidence, not a punishment for success.** The player can see rising concern. Mobilization takes time, is budget-limited, can be misdirected, and may yield competition, partnership, procurement pressure, acquisition offers, or an abandoned effort.

### Politics, economy, and public effects

The US simulation models a president, House and Senate seat blocs, committees, elections, agencies, courts as abstract review, state governments, and organized interests. Elections occur on recognizable two/four/six-year cycles without modeling every legislator as a character. Fictional parties/coalitions hold issue positions on privacy, innovation, labor, competition, public investment, security, and access.

Policy proposals move through agenda, committee, legislative votes, executive decision, implementation, and possible review. Scope and effective dates matter. The player can testify, submit evidence, build coalitions, fund lawful advocacy, negotiate public procurement, comply, contest an interpretation, or change a product. Influence has visible costs, disclosures, and diminishing returns. It cannot simply buy an election or erase all institutions.

International profiles model market access, public investment, trade restrictions, standards, and diplomatic tension. An international coordination forum can propose standards and treaties; adoption depends on member interests and domestic implementation. No magic world-government law instantly changes every region.

The economy has demand, capital conditions, employment opportunity, energy capacity, public services, concentration, public confidence, and regional distribution. The whole AI sector and public institutions affect them. Company actions change intermediate outcomes with lags: deployment → adoption/productivity → income/distribution/services → sentiment/politics → budgets/rules. A national labor transition cannot be repaired instantly with one ad campaign.

## 10. Eras, AGI, ASI, and endings

2014–2026 content is inspired by documented research and hardware eras, using fictional vendors and game statistics. Public discoveries in this reference interval have availability windows; investment can change local adoption and occasionally bring a research direction forward within a bounded window. The player is not forced to reenact real corporate history. The simulation is alternate history from its first day.

After 2026, each seed combines progress rate, energy availability, social adaptation, cooperation, and research uncertainty. There are no scheduled AGI/ASI arrival years. Progress can be fast, uneven, slow, or plateaued. Hardware concepts are opportunities with bottlenecks, not promises that quantum or photonic devices solve all problems.

**Game AGI** means broad competence across the game's domains plus sustained reliable transfer on unseen tasks. **Game ASI** adds a higher sustained research/engineering threshold and a demonstrated scalable improvement process. These are fictional operational definitions, not scientific claims or consciousness tests. Evaluation gates and resource commitments are explicit. Capability, controllability, social benefit, and legal permission are separate properties.

Research assistance can accelerate experiments and software work subject to verification, physical lead times, compute, energy, data, and organization. No instantaneous infinite recursive improvement. High autonomy with weak control can create systemic risk. Catastrophic endings require escalating evidence and opportunities to reduce exposure. Their presentation is a short, non-graphic bulletin and a campaign review.

### Objective rules

Select one primary objective and optional secondary goals. Progress is visible with exact definitions; changing the primary objective marks the run as a custom sandbox. The ASI race may conclude early. All other victories are assessed at the century deadline using sustained conditions, so an early lead is not automatically permanent.

| Objective | Winning condition |
| --- | --- |
| World Monopoly | At least 85% of useful AI task units delivered globally across the final five years, with at least 60% in every world region. Includes licensed deployments with a defined attribution rule. |
| World Domination | Durable governing authority over regions representing at least 80% of world population **and** economic output, with at least 60/100 authority in each covered region, for the final five years. Soft institutional control or abstract hard coercive control can qualify; coercion carries resistance and failure risk. |
| World Utopia | For the final ten years: global basic-needs access ≥90, health access ≥85, civic agency ≥80, ecological resilience ≥80, and shared prosperity ≥75; every region ≥70 on each; no world-ending catastrophe. Values are disclosed fictional indices. |
| Profit | Highest founder-legacy net wealth among competing institutions at the deadline, measured in constant 2014 dollars without counting the same company equity twice. |
| ASI Race | First institution to complete and independently validate the game ASI gates. Unsafe development can qualify; safe operation is a separate campaign distinction. Same-day qualifiers share the win. |
| Simple Survival | The founding institution is operating and solvent at the deadline, with an active organization and meaningful product/research activity. Founder succession, an IPO, and approved mergers under retained institutional control are allowed. |

An ASI objective uses a scenario family with at least one feasible ASI route; plateau-only scenarios explain that incompatibility in setup. Utopia can be reached through broadly distributed useful AI and institutions without ASI. Financial success does not automatically imply political control or social benefit.

The objective panel distinguishes progress from whether the deadline remains attainable. A failed mandatory month in the final Domination/Utopia window immediately explains the irreversible breach and auto-pauses by default, offering a postmortem, continued play or a custom sandbox. A bad Monopoly month alone does not imply failure: that objective uses aggregate work over five years. Do not force players through years of a known lost objective. Default thresholds/windows remain strict; early rolling victories or grace months are separate balance choices, not hidden exceptions.

Failure conditions are unresolved insolvency after a warned 30-day workout, complete loss of institutional control, final closure under enforceable orders, or a scenario-ending catastrophe. A takeover offer, bad launch, executive departure, or lost election is not automatically game over. Postmortems show causal chains, unfulfilled obligations, lessons, and alternative actions that were available at the time.

## 11. Interface, atmosphere, and accessibility

The **Office** is the hub: cash/runway, service health, current projects, inbox, calendar, objectives, and a next-decision summary. The **Lab** handles research/data/models/evaluation. **Operations** handles hardware, capacity, and sites. **People & Board** handles staffing and governance. **Finance & Deals** handles accounts, products, and negotiation. **Civic & Media** handles policy, testimony, and public communications. The **Map Room** links domestic sites and the wider world. Banks, investors, hearings, and meetings open contextual location sets rather than creating additional navigation chores.

All rooms share a compact status bar, time controls, searchable command menu, alerts, back navigation, and persistent context inspector. Every important table supports sort, filter, units, and comparison. Room hotspots have text/keyboard equivalents. A player can complete the game through a compact management view without hunting visual hotspots.

The current executive has a physical base and a calendar. Optional site visits, investor meetings, public appearances, and hearings can require scheduled travel between states, with visible time/cost and a remote/delegated alternative where appropriate. Presence can improve inspection evidence or a relationship; it does not unlock basic management screens. The player can keep managing the company while a trip is in progress. Travel is a strategic appointment choice, not a walking minigame or a routine navigation toll.

Use restrained stylized illustration, warm office materials, readable paper/display surfaces, and a changing technological atmosphere. Early rooms are small; a powerful late institution is expressed through context, scale, and information rather than expensive cinematic assets. Typography and charts carry most of the work. Never copy Plutocracy's art, layout, text, names, or proprietary UI assets.

Target 1280×720 minimum, resizable windows, 100–200% text/UI scaling, high contrast, remappable shortcuts, full keyboard navigation, visible focus, reduced motion, non-color status cues, and independently adjustable music/effects. No timed dialogue choices. Critical events have text summaries. Accessibility labeling and screen-reader behavior are tested on actual platform builds before claiming support.

Music is licensed existing music, selected primarily from ENDE.APP: coherent ambient/electronic business atmosphere, restrained tension, short achievement cues. No generated soundtrack and no required voice acting. Silence is a valid full-featured play mode. All credits are available offline.

## 12. Content and replayability

Events combine systemic triggers with authored characters and specific dilemmas. Choices expose immediate commitments, plausible benefits, uncertainty, and affected audiences; hidden outcomes may exist, but hidden arbitrary penalties do not. Each major system needs opportunities, recoveries, and tradeoffs as well as crises.

Example: a supplier offers 40% cheaper compute for exclusivity, a board observer, and a three-year take-or-pay contract. The CFO favors runway relief; product leadership worries about lock-in; the chief scientist wants guaranteed capacity. The player may accept, negotiate volume/exit terms, choose a smaller plan, or delay the next model. Later power constraints or changing demand determine whether the deal was wise. No choice is scored as intrinsically virtuous.

Replayability comes from rival strategy, geography, discovery pathways, demand, funding conditions, institutional choices, objectives, and world response. Authored scenario packs may add crises, starting conditions, or constraints after the base game. No procedural text generation or external service is needed.

For minimum content counts, example event data, privacy rules, release criteria, and development order, use [content and assets](CONTENT_AND_ASSETS.md), [QA and release](QA_AND_RELEASE.md), and [the roadmap](ROADMAP.md).

## 13. Human-authored content and the separate Studio

Game content is produced with a separately downloadable graphical [Content Studio](CONTENT_EDITOR.md), whose source lives in the repository. It edits the repository's current definitions, text, scenarios and asset records. The base game ships only reviewed runtime content and has no editor, player mod manager, remote content service or authoring dependency.

Authors connect trigger boxes to ordered effects/events, conditions, dialogue choices/results, delays and reusable flows. News, player dialogues and hidden logic can be once-only, recurring, scheduled, interval-based or continuously active as appropriate. A searchable library organizes existing content; forms/tooltips, a node graph and equivalent outline expose the same definition.

The author can inspect why a trigger matches, its scope, which path runs, and before/after changes for each effect. Unit/sign/target checks, boundary examples, reference tracking and test previews address scripting mistakes such as inverted effects or mixed-up events. The Studio uses the same runtime as the game. Humans still judge creativity, meaning and balance.

The Studio has version controls for finalized content batches, with separate game/editor/content identities and save-compatibility tracking. Later Snap packaging reads that same metadata; content editing does not publish a build or add an in-game updater. [Versioning contract](VERSIONING.md).
