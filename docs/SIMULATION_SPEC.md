# Simulation and balance specification

Baseline 1.2 · 2026-11-03 · Implementation contract, with provisional balance constants

Read alongside [the game design](GAME_DESIGN.md). The simulation models fictional companies and simplified world systems. Its formulas support interesting, understandable choices; they are not predictions of actual AI development, investment returns, elections, or catastrophic risk.

## 1. Time, units, and reproducibility

One authoritative step is a day. The normal decision snapshot dated `t` is that day's morning briefing, after opening deliveries/rules/flows but before staff/compute assignment and work. Advancing one day finishes activity over `[t, t+1)` and prepares the next morning. The campaign stops at 2114-01-01 after settling 2113-12-31; it does not execute a new day's deliveries or work. Weekly reviews occur every seventh day from the campaign epoch; monthly and annual settlements follow the Gregorian calendar. All deadlines name a date and a phase, avoiding ambiguous inclusive end dates. Save the day cursor as well as the date: a partially resolved morning is different from a morning already prepared for work.

| Quantity | Stored unit | Display |
| --- | --- | --- |
| Money | Signed checked 64-bit integer constant-2014 cents | Dollars labeled “2014 purchasing power” throughout forecasts, contracts and objectives |
| Population/requests/tasks | Checked integer counts or explicitly named thousand-unit counts | Counts with unit labels |
| Capacity rate | Checked integer `milli_CU_per_day` | CU/day; one fixed scenario reference across eras, not advertised FLOPS |
| Work consumed | Checked integer `milli_CU` | CU; a day's rate integrated over simulated days |
| Energy | Integer watt-hours | kWh/MWh |
| Power | Integer watts | kW/MW |
| Skill, confidence, trust, quality | Integer 0–10,000 | 0–100 with labels and uncertainty where appropriate |
| Probabilities | Integer parts per million | Percent or verbal band |
| Shares and rates | Basis points unless a schema names a different scale | Percent |
| Time/duration | Integer ordinal day and integer days | Date and duration |

Use distinct value types for `Cents`, `MilliCU`, `MilliCUPerDay`, `BasisPoints`, `PartsPerMillion` and `OrdinalDay`; conversions require an explicit named operation. All money fields ending in `_cents`, including authored thresholds, are constant-2014 cents. Rates and signed rate deltas have separately validated ranges.

Arithmetic checks additions, sums, products, denominators and final narrowing. `mul_div_round_half_up(a,b,d)` computes the exact nonnegative product before one division/round, rejects `d <= 0`, and refuses a final result outside the destination range. Multi-factor helpers likewise divide once by the full scale, not after each factor. Use a checked wide intermediate with sufficient range for validated inputs (128 bits suffice for the bounded formulas here), plus a portable multiword implementation/fallback on targets lacking that type. MSVC x64 multiplication intrinsics alone are not a Windows ARM64 implementation and do not supply division. Signed postings use a sign and an unsigned rounded magnitude, including safe handling of the most-negative input. Never wrap or silently saturate financial values. Allocation quotas use floor plus largest fractional remainder, with stable entity-ID ties and constraint rechecks, rather than independently rounding every recipient upward. No binary floating-point enters authoritative arithmetic or JSON state.

Use a documented integer PRNG algorithm with independent streams for research, rivals, markets, geography, politics, hazards and narrative events. Save each stream state. Bind each stateful RNG draw to its own statement before passing values to a function; C++ does not specify a portable function-argument evaluation order. Every outcome-relevant sort has a total key ending in a unique ID/composite key; equivalent scores cannot rely on a library's sort order. Do not use `std::hash`, library-dependent random distributions, current time, wall-clock duration, or unordered container iteration to decide outcomes. Integer lookup tables or rational approximations replace platform-dependent transcendental functions. Rendering and forecast previews never consume authoritative randomness.

Reproducibility means identical simulation version, content hashes, seed, and command sequence give the same state hashes on every supported architecture. New balancing content can change outcomes only under a new content version. A later version can migrate a save without promising identical continuation under changed rules.

## 2. Daily transaction order

1. Begin a date once, loading its saved phase cursor and previously committed command receipts. Do not replay a completed opening or accept a work command against yesterday's rules.
2. Apply scheduled deliveries, effective regulations, contract transitions, exogenous conditions, due content flows, and continuous-flow entry/exit/modifiers once. Resolve shared resource allocation from a common snapshot rather than company iteration order. Publish the morning briefing and stop for required choices/alerts before work. A blocking opening dialogue can suspend this phase; resume its saved continuation before exposing the work-ready boundary.
3. Assign available staff and compute according to validated policies. Include serving commitments, maintenance, and outages.
4. Advance research, data processing, training, and evaluation. Newly completed models can be deployed by a command on the next day, not retroactively.
5. Allocate contract demand and discretionary demand simultaneously; serve capacity-limited useful work; book revenue, refunds, service costs, and contractual penalties.
6. Accrue payroll, power, debt, taxes, and other obligations. Settle amounts on their due dates; run liquidity warnings and workouts.
7. Perform due weekly/monthly/annual updates: rival strategy, adoption, macro effects, politics, staffing reviews, and hazard sampling.
8. Generate event facts and consequence explanations, evaluate end-of-day reaction triggers, queue their authorized work for the next opening boundary and update objective accumulators. Increment the date and prepare phases 1–2 for the new morning before publishing a work-ready snapshot. At the campaign end boundary, publish the final result instead. A manual pause requested during work takes effect at the next safe morning; required choices can pause within opening resolution.

AI decisions are commands using the same validation rules. A decision requiring a player response pauses before its deadline; unresolved optional correspondence follows the disclosed default. Async jobs may prepare calculations, but only the simulation thread commits results in canonical order.

Between phases 2 and 3, validate and commit queued player/AI work commands in recorded order against the prepared morning. This command stage may drain while paused, so a choice shows its committed result without advancing a day. Opening-dialogue response commands resume only their waiting transaction/flow; ordinary work commands wait until the morning is ready. Commands and required modifier updates do not resample calendar/recurrence checks or repeat receipts. Rendered frames never advance effects. Starting a fresh campaign also prepares its first morning exactly once.

Reversible allocations can be undone before work starts if they have not consumed resources, disclosed information, transferred rights or triggered dependent commitments. Release their reservations through a validated cancellation command. A data license delivered immediately, a signed supplier obligation, completed hire, public release or random result is not refundable merely because the date is unchanged. Preview/confirmation states name any irreversible cost; contract-specific cancellation/refund terms govern actual payments. This replaces a blanket zero-day refund rule and prevents free-information or purchase/cancel exploits.

## 2a. Content-flow runtime and graphical authoring contract

The [Content Studio](CONTENT_EDITOR.md) and game share one typed content compiler, evaluator, and effect executor. A content flow has a stable ID/revision, typed scope/bindings, trigger, lifecycle, and ordered graph. Trigger boxes may lead to several effects or news/dialogue events, conditions, choices/results, delays, reusable presentations, and terminal nodes. A referenced presentation has no second automatic trigger. Hidden logic uses the same runtime without a visible window.

Conditions use nested all/any/not, typed comparisons and explicit temporal predicates over registered facts. Facts expose type, unit, scope, phase and help metadata. There is no implicit number-to-boolean, money-to-rate or entity-type coercion. Eligibility inspection returns each clause's actual value/result and selected scope; unsupported facts, unresolved bindings, or malformed comparisons are validation errors, not false successes. Player-facing explanations respect what the player knows; the authoring fixture inspector can view full test state.

Registered typed variables/flags, checked arithmetic expressions, related-entity selectors and bounded any/all/count quantifiers support reusable rules. Collection traversal is stable and a selector reports its resolved members/count. Duration predicates persist their start/reset state. Weighted random branches use the content stream and persist their draw/selection; they do not evaluate randomness while merely inspecting a trigger. Variable namespaces/defaults and target/occurrence scope are explicit, preventing accidental global flags.

Daily `holds_for_days` predicates measure elapsed observed days: first true on
day D starts at zero and satisfies N on D+N; a false sample resets the run and
missing observation dates restart it. Sample all enabled roots' trigger and exit
predicates after core opening work, before content effects, independently of
cooldowns, interval eligibility or active continuations. Nested predicates sample
inner before outer; boolean short-circuiting cannot suppress a reset. Exit
predicates measure the scope condition's history, including before activation
and across reentry; they do not imply activation age. The saved daily sample is
frozen through choices and repeated preparation. Later content effects are
observed the next morning. Inspection reads saved history without updating it;
player inspection hides timing/history when any descendant fact is hidden.

Lifecycles are once, recurring, interval, scheduled and continuous. Once/limits/cooldowns use an explicit occurrence scope. Recurring specifies re-entry or cooldown-while-still-eligible semantics. Intervals use one typed anchor: `campaign_start`, `scope_created`, `month_start`, or `fixed_date` with an ISO date, plus an explicit first-due offset. Calendar-month anchors name day-of-month and end-of-month clamping; `month_start` means the first calendar-month boundary on/after scope creation, then the offset. Resolve and save anchors at activation; skip ineligible checks without later backfilling. Scheduled instances define delivery eligibility, expiry and captured versus delivery-time target resolution. Continuous flows register owned modifiers on entry and remove them on exit; they do not repeatedly alter a base value or undo a percentage with its negative. Entry/exit hysteresis and stacking are explicit.

Begin-day due work executes in phase 2, before capacity/cost calculation. End-of-day reaction triggers observe committed daily facts and schedule next-boundary effects; they cannot change work already performed. Evaluation uses fact-change subscriptions plus due-time queues, with a defined daily fallback for temporal predicates. Stable ordering is phase, due date, explicit priority, scope ID, definition ID, instance ID and node order. Fan-out outputs are numbered sequences, not parallel writes. A flow may not recursively mutate facts and re-trigger itself to a fixpoint within the same phase.

Within a resolution segment, typed effects commit as one validated transaction or produce a rejection with no partial effect. Choices/delays separate segments; later failure cannot roll back an already accepted earlier choice. Resource-grant/transfer effects require an authorized source and balanced accounting. New capabilities require registered tested core operations, not arbitrary embedded scripts.

A sequence has ordered child subflows and an explicit continuation/return boundary; each child completes (possibly after a decision/delay) before the next begins. Persist a current node plus a bounded continuation stack, initially at most eight frames across nested sequences/subflows. A frame records its sequence/subflow ID, next child index, return node and binding context; the current node alone cannot resume nested work. Validate nesting at compilation/load and refuse excess depth with a trace. Cross-sibling jumps are invalid. Independent future activity requires explicit scheduling effects. An end node returns to the top frame or ends the activation when none remain; a continuous instance's active state/modifier ownership persists until its exit lifecycle resolves. Lint a delay/decision in an early numbered branch when later branches wait, showing that timing without assuming it is a mistake.

Persist activation IDs, definition revisions, captured scope/context, current nodes/continuation frames, committed-effect receipts, selected choices, pending results, delays, occurrence counts, cooldowns, active modifier keys and relevant RNG state. Loading or reopening cannot duplicate an effect or reroll an outcome. Removed/merged scope entities follow an explicit cancel/transfer policy; reused IDs never inherit unrelated pending work. NPC choices follow their controller's policy and the same prerequisites/costs. Normalize every effect node to an `effects` array with an explicit typed `target` on every item (including `self`); authoring forms may suggest `self`, but unresolved/omitted targets are not runtime defaults.

Reject immediate graph cycles, dangling links, impossible port types and unbounded recursive subflow expansion. Intentional delayed loops require positive delay and occurrence/termination bounds. Budget evaluation/steps/queue growth deterministically; exceeding a hard runtime budget pauses with a recoverable content error and trace rather than silently dropping work, shifting dates or hanging. Bound checks must not depend on CPU speed. Test fixtures cover boundary values, stale choices, save/reload at every lifecycle transition and deliberately reversed effects/wrong targets.

Trigger visualization and before/after previews execute on copied valid test state with independent copied RNG streams. Pure fact overrides can inspect conditions but are labeled as trigger-only evidence. Shared traces show resolved target, matched scopes, ordered path, committed delta/ledger posting and queued future consequences. A useful visualization explains conditional reach and uncertainty; it does not claim exhaustive proof of long-run impacts. See [versioning](VERSIONING.md) for content identity and pending-chain migrations.

Continuous-preview fixtures cover entry, an unchanged active tick, and exit through the normal lifecycle evaluator. Supply a valid fixture transition that satisfies exit conditions; merely advancing a day need not do so. A separately labeled synthetic cleanup probe may force removal on copied state, but proves only cleanup, not natural exit eligibility/timing. Compare this modifier's contribution and preserve concurrent base/other-modifier changes; do not demand every field return to an old absolute baseline. Re-entry and reload must not duplicate ownership.

## 2b. Built-in event sources and news

The [event-system contract](EVENT_SYSTEM.md) defines the playable baseline and extension rules. Owning systems publish typed facts only from committed results, with a source receipt/occurrence, scope, date/phase and knowledge visibility. Registered detectors use changed facts and due-time boundaries to identify major developments, threshold entry/re-entry and scheduled/seeded world incidents. They activate the existing shared flow runtime; they do not supply another effect executor or apply a completed system result twice.

An occurrence key connects its cause, semantic fact and scope to consumed activations, optional story layers and news. Persist that identity, selected random outcome/bindings, occurrence limits, cooldowns and pending flow state; reloading, revisiting an inbox item or changing a definition revision cannot repeat the same underlying consequence. A story declares presentation replacement or explicit follow-up to the existing occurrence. Baseline and story cannot independently charge or reward the same incident.

Seeded incidents declare integer chance/weights, eligibility, cadence, bounds, no-event outcome and expiry in data. Use the owning system's saved stream for its sampling, and narrative/content randomness only for narrative branches; producing a headline draws nothing. End-of-day detections queue next-opening work; scheduled opening occurrences follow phase 2. News projects actual committed outcomes with player-visible causes. Group ordinary developments in a digest while retaining source records and required decisions/deadlines. No live news ingestion or runtime AI is part of simulation.

## 3. Opening balance and the first experiment

The vertical slice covers 2014-01-01 through 2017-01-01. It offers three headquarters choices (California, Texas, Washington), cloud leasing, six research nodes, four dataset offers, one enterprise contract family, one product channel, two startup rivals, and one heavyweight with a limited warning/mobilization behavior. It is a three-year game with a complete start, tutorial, recovery route, and ending.

Initial normal-difficulty balance, before player changes:

| Item | Value |
| --- | --- |
| Cash | $1,200,000 |
| Staff | Six people including founder, abstracted into research/product/operations capacity |
| Payroll including benefits | $72,000/month |
| Office/admin | $18,000/month |
| Compute lease | $18,000/month for 100 CU/day |
| Baseline recurring cash cost | $108,000/month; approximately 11.1 months of runway without revenue |
| Cap table | Founder 55%; cofounder 20%; seed investor 20%; option pool 5% |
| Board | Three seats: founder, cofounder, seed investor |
| Initial customer opportunity | $25,000/month at acceptance; $5,000/month incremental serving/support cost |

A basic training run reserves 60 CU/day for 60 days: **3,600 CU**. It leaves 40 CU/day for other work and incurs a **$12,000 one-time data license**, not another charge for the already leased compute. Power is included in that lease; owned hardware would incur a separate power bill. Two months without revenue leave about **$972,000** after baseline costs and the data license. Once the contract operates, net monthly cash burn is approximately **$88,000** before new commitments. Forecasts prorate by actual calendar days; these round monthly figures illustrate the accounting rule.

At acceptance the example contract promises 1,000 useful document tasks/day, at 20 `milli_CU` per task: reserve 20,000 `milli_CU_per_day` (20 CU/day) for inference. A later 60 CU/day training project can coexist, leaving 20 CU/day free. Serving begins only after a qualifying model exists. The $5,000/month incremental cost is non-compute support/operations in this example; compute is already paid by the lease. Do not charge it twice. These task/cost values are provisional balance inputs, not measured model performance.

The slice must be survivable by a careful specialist strategy without a new funding round, and a financing strategy must also work. This is an acceptance target to establish by simulation/playtest, not an assertion that the initial values already prove a three-year balance. A smaller prototype, staged customer payment, staff changes, and profitable contract expansion provide recovery routes.

## 4. Ledgers, capacity, and markets

### Financial rules

Every posting has day, debit account, credit account, amount, cause, and owning entity. The accounts model includes cash, receivables, assets, accumulated depreciation, payables, debt, equity, revenue, and expenses. Financial reports derive from postings; UI values cannot edit balances. Cash reservations are commitments, not recognized expenses until the relevant transaction occurs. Credits, deposits, and equity proceeds are not product revenue.

Liquidity warnings inspect committed settlements. If a payable due through the
completed day remains unpaid, or matured debt retains principal, the next morning
W presents a causal warning and pauses for acknowledgment. Negative profit or
zero cash alone does not start insolvency. The saved response deadline is W+30
calendar days; commands may rescue the company on that morning. Advancing with
unresolved exposure closes the company at W+30 before another service interval,
without new work, income or costs. Partial payments and acknowledgment
preserve the original deadline. A full cure resolves the workout immediately;
funding without servicing the liability does not. Closure stops further work
and preserves ledgers, obligations and history for a postmortem.

All balances, prices, budgets, wages, debt, interest-rate assumptions and authored money thresholds use constant-2014 purchasing power. There is no compounding nominal price index or nominal-dollar ledger in 1.0. Real scarcity, productivity and financing shocks change explicit authored real prices/rates. Do not add a nominal display index or cash-decay mechanic implicitly. Interest and monthly recurring payments accrue using specified day-count rules; the first implementation uses actual days in the contract's calendar month. Taxes are fictional policy parameters on explicit taxable bases, not reproductions of any jurisdiction's current tax code. Carries, relief, and incentives have caps, eligibility, and expiration.

All equity allocations sum to 10,000 basis points. Dilution changes ownership and does not automatically change seat allocation. A merger conserves assets, debt, obligations, licenses, and control rights; acquired closed products do not instantly become compatible. The first version supports cash/equity purchases and straightforward mergers, not arbitrary corporate derivatives.

The opening option pool is a reserved part of the fully diluted cap table, not a second 5% ownership grant. Cash-only or negotiated cash-plus-equity compensation reserves grants from its unallocated portion. Track each grant's beneficiary, basis points, start/cliff/vesting schedule, vested amount and departure terms; team grants remain aggregated. Vesting transfers ownership within the reserved pool and cannot dilute twice or create cash. Calculate cumulative vested entitlement, then book only its increment so rounding cannot duplicate shares. Unvested departures return the reserved remainder; vested claims remain outstanding under the agreed terms. Expanding the pool needs the required board approval and explicit pro-rata dilution, conserving 10,000 basis points with stable rounding. Do not reduce payroll without agreed compensation terms or model option taxes/derivatives in 1.0. M4 adds this policy; the slice keeps the pool reserved.

### Shared physical capacity

Daily effective capacity is the minimum allowed by installed equipment, interconnect/memory requirements, staffing, grid/lease allocation, and availability. A project reserves a feasible slice of that capacity. Inference has a configurable protected minimum; overcommitment either queues work, slows training, or refuses a new commitment according to policy. The same hardware cannot be counted as cloud and owned capacity simultaneously.

Hardware output uses the same CU reference throughout a scenario. Each hardware/recipe has data-defined memory and interconnect requirements/classes; incompatible jobs cannot run merely because their scalar CU fits. Bound catalog output, per-task costs and research scales for century/sandbox arithmetic, then tune their progression through playtests. Do not mandate a 1,000× growth curve or six universal tiers without balance evidence. Legacy hardware may still serve compatible specialist models while maintenance/power determine its value.

Every model/task-family serving profile defines positive integer `inference_milli_CU_per_task` per useful-task equivalent, including its modeled retry/control overhead; select a larger explicit task batch if the cost is below one stored unit. `required_milli_CU = delivered_tasks × inference_milli_CU_per_task`; capacity-limited tasks are `floor(available_milli_CU / inference_milli_CU_per_task)`. Separately modeled wasted/failed work consumes capacity and receives no useful-task credit; do not also charge overhead already included in the profile. Compute this against the same daily pool as training/evaluation/other reservations, with checked arithmetic. Non-compute support costs have separate per-task or fixed-monthly terms. Efficient-serving research can reduce future profiles' cost within a declared floor, never to free unlimited work. Local/on-premise tasks consume the buyer's modeled capacity; the vendor must not also bill itself for the same inference.

From M5, wholesale compute contracts can reserve deliverable owned-site capacity for a buyer for a stated term, price, compatibility, energy inclusion, uptime and curtailment/refund policy. The seller cannot use or resell that reserved slice; the buyer receives it once through the shared capacity ledger. Supplier revenue and buyer expense/payables reconcile. Leased capacity cannot be sublet without explicit supplier rights, and no circular resale creates capacity. Wholesale revenue is distinct from the six model distribution channels and creates no AI task-share credit for the host; attribute actual work once to the deployed product's vendor. This closes the infrastructure-provider strategy without enlarging the slice.

Owned-site energy per day:

`energy_Wh = (active_IT_watts × active_hours + idle_IT_watts × idle_hours) × PUE`

Use fixed-point PUE ≥1, include cooling once, and apply backup power/water/maintenance costs through separate typed inputs. Delivered CU depend on hardware efficiency and utilization, not energy price. Electricity cost is energy in kWh times contracted price; lease prices state whether energy is included. Grid capacity and construction lead times are finite. Curtailment reduces work performed and associated energy rather than charging for imaginary full utilization.

### Customer allocation

Each region/segment has potential useful-task demand, a budget, incumbent workflows, and requirements. Contract obligations reserve demand first. Discretionary demand compares eligible products and an outside option (using no AI or keeping an existing non-AI process).

For eligible product `p` in segment `s`, compute a nonnegative integer utility from task fit, reliability, latency, trust, support, switching cost, visibility, and price relative to the segment budget. All coefficients are data. Weight allocation by the square of positive utility above the reservation threshold. Include the outside-option weight in the denominator. Enforce willingness-to-pay and legal/technical eligibility before allocation.

Save installed useful-task shares by region/segment/product at monthly adoption reviews. Switching costs apply to movement away from that installed base, rather than as a uniform penalty on every vendor. Channel-specific churn bounds apply to discretionary migration; outages, legal exclusion and contract expiry may force departure. No churn rule forces purchases from an unavailable vendor. New sales and usage still obey daily budgets/capacity.

Allocate from a common snapshot, reserving contractual tasks/fees first without charging the same customer budget twice. For discretionary work:

1. Filter eligibility, explicit per-task willingness-to-pay, and positive serving capacity. Derive utility weights and include the outside option. Compute task quotas with checked wide `remaining_tasks × weight / total_weight`, retaining rational remainders.
2. Enforce each segment's remaining money as well as task count. If tentative cost exceeds budget, scale quotas by the rational ratio `budget / tentative_cost` **before** integer division. Never compute that ratio as an integer first: 100 cents / 150 cents would incorrectly become zero. Recheck every residue assignment against money, demand and capacity; zero-price work consumes tasks/compute even though it consumes no customer money.
3. Resolve competing segment claims on each product's capacity together. Use a disclosed policy (contract priority, then proportional discretionary allocation initially), with stable-ID remainder ties. Reallocate refused tasks only with the remaining budget and supply; finish unallocated tasks in the outside option and record why they were unserved. Bill only delivered/contractually owed work, refunds and penalties.

M2-03 must freeze a finite allocation procedure and its deterministic termination/work bound on validated catalog sizes, backed by hand-worked unequal-price, free-product and cross-segment contention fixtures. An arbitrary eight rounds is not a convergence proof; an exhausted hard computation budget raises a recoverable traced error rather than pretending allocation succeeded. This specification fixes constraints and rounding without claiming an unimplemented market optimizer is complete. Preserve total tasks/money; advertising changes awareness with diminishing returns, not capability or guaranteed purchases.

Useful-task units are scenario-defined equivalents by task family. They measure successfully completed work, not raw tokens or model size. The denominator includes all active vendors in all regions; the outside option is reported separately and does not count as delivered AI work. Publishing a free model produces no delivered share merely from downloads. Deployed open/on-premise systems generate modeled work attributed once to their primary vendor, with component suppliers excluded from the same market-share numerator. Closed and open distribution use the same attribution rule.

## 5. Models, data, and research

### Resource-to-capability model

Let `sat(x,k) = floor(10,000 × x / (x+k))`, with nonnegative quantities and positive tuning constant `k`. It provides diminishing returns without logarithms. Architecture/recipe definitions specify a domain baseline, a maximum training gain, compute/data scales, memory/interconnect constraints, and serving costs.

For domain `j`:

`effective_data_j = checked_sum(round_half_up(usable_unique_units_i × quality_bp_i × relevance_bp_ij × coverage_bp_ij / 10,000^3))`

`bottleneck_bp_j = min(sat(consumed_milli_CU, compute_scale_milli_CU_j), sat(effective_data_j, data_scale_units_j))`

`capability_bp_j = min(research_ceiling_bp_j, architecture_baseline_bp_j + round_half_up(training_gain_bp_j × bottleneck_bp_j × recipe_fit_bp_j / 10,000^2))`

All `_bp` factors are basis-point values; each displayed expression divides once by its full scale using checked wide intermediates. Effective data is rounded once per source contribution before the checked sum. Compute scales and consumed work are both total `milli_CU`, not daily rates or whole CU. Check `x+k` and the saturation numerator before evaluation. Research ceilings, recipe fit, and gain coefficients are scenario data. The formula expresses complementary bottlenecks; it is deliberately not an empirical scaling law. Actual research motivates the distinction between compute, data, and parameter choices, not these game coefficients. See [research sources](RESEARCH.md).

Rights and consent affect cost, availability, contracts, litigation, trust, and governance; they are not hidden direct deductions from mathematical training capability. Quality and rights can correlate in an authored offer but remain independent attributes. Data contamination affects reported evaluation, not genuine capability. Synthetic-data quality depends on parent capability, diversity, filtering, and external verification, with increasing duplicate/coverage limits.

A run's score is computed from its recipe and accumulated effective resources. Continuing a checkpoint updates that accumulation; repeatedly cloning/restarting a checkpoint does not add the previous score as a new bonus. Transfer learning may preserve a documented baseline and reduce required work, subject to a recipe-specific ceiling. Specialist fine-tuning can reduce performance elsewhere. Checkpoint lineage and data rights follow every derived model.

### Evaluation and deployment

Maintain true simulation capability, measured estimates, coverage, uncertainty, and observed field performance as separate fields. The player sees evidence and an interval; the simulator needs true values for customer outcomes. More suitable independent tests narrow uncertainty. Buying an audit cannot directly raise the underlying score.

Make discovered limitations concrete: an evaluation finding names its task/subgroup, observed failure, evidence coverage and severity (for example, unreliable scanned tables). Latent failure modes remain simulation state until relevant tests/field evidence reveal them; a test does not reveal every hidden defect. Show possible responses: acquire targeted data and retrain/re-evaluate, add a matching human-review/control cost, or narrow the promised task scope. These are the existing evaluation/deployment mechanisms, not a separate collectible flaw system, and no action guarantees a fix by removing a label.

Reliability, resistance to distribution shift, controllability, operational monitoring, and permission boundaries are separate axes. Deployment controls reduce specific failure modes and may change cost, latency, or eligible markets. For example: human approval reduces unsupported autonomous actions and throughput; stronger evaluation improves estimates and catches defects before rollout; monitoring shortens response time after a detectable incident. Do not reduce all controls to an additive safety percentage.

Incident probability is a scenario-authored integer lookup over deployment scale, exposure, autonomy, task severity, capability-control mismatch, and relevant mitigations. Monthly sampling uses the risk stream; schedule the incident within the next period. Independent local outages use site hazard tables. Rare world-ending chains require explicit precursor conditions and a documented sequence of escalations; no unavoidable random nuclear game-over roll. Numbers are fictional, with visible risk bands rather than fake precision about actual AI risk.

### Research implementation

A research definition contains prerequisites (`all_of`, `any_of`), field, required knowledge, staff-days, experiment CU, cash, maturity, outputs, failure-learning credit, and allowed publication choices. Progress accrues from qualified effective staff-days and completed experiments. Changing teams causes a bounded handover cost. Research hazards depend on project/knowledge state, not how often the player presses a button.

Initial six nodes: efficient image features; sequence modeling; data deduplication; held-out evaluation; efficient serving; reproducible experiment tooling. Later groups introduce attention-based architectures, multimodality, preference adaptation, tool use, retrieval, verification, mechanistic analysis, robust deployment, advanced hardware/software co-design, and speculative broad research systems. Nodes create capabilities and constraints for recipes/products. The content team expands the graph only after the slice loop works.

A topic can specify a first-experiment setback, a safeguard topic,
a finding and a retry credit. The outcome resolves when actual work settles,
using a deterministic sample keyed by the original research seed and topic
ID. Only safeguards completed before the attempt count. Cancelling leaves the
topic's first result unresolved. After a setback the next settled experiment succeeds
with a bounded compute credit. The larger of the public and retained
credits applies, subject to the full staff-day floor. A failed result
cannot satisfy a prerequisite or support publication.

## 6. Organization and opponents

Teams have skill capacity, salaries, hiring pipeline, workload, morale, and knowledge retention. Effective staff-days are available people-days times task fit and a bounded morale/coordination factor. Adding people has onboarding and coordination costs. Burnout accumulates with sustained workload and recovers through time and working conditions; bonuses alone do not erase it instantly.

Specialist offers can specify calendar days to payroll start and paid days
of onboarding. Signing posts the fee and records a dated salary commitment.
The prepared morning after onboarding adds one available staff member.
Withdrawing operations before readiness ends the commitment and preserves
signing costs and salary already accrued.

Explicit evaluation assignments reserve an onboarded specialist within the
chosen team size. Their authored expertise reduces required experiment CU
by up to 25%, rounded up once. Plan fees, coverage and evidence calculations
use their ordinary terms. The saved assignment cannot overlap another
unfinished evaluation, including a paused test. Completion or cancellation
releases the specialist.

The current executive's base, appointments, travel days and delegated authority are explicit calendar state. Trips reserve executive availability and expense without disabling remote company management. On-site inspection improves only the evidence actually observable there; a meeting can affect a relationship without guaranteeing a deal. Scheduling must detect overlapping commitments and allow a remote/delegated alternative where the appointment permits it.

Executive confidence is a weighted record of delivered results, compensation, autonomy, values, and promises. A major dispute is a state machine: concern → formal recommendation → negotiation/deadline → resolved, departure, board motion, or disclosure. A disclosure requires knowledge of a concrete issue and a failed response path; it is not simply a personality die roll. Promised appointments, publication rights, and safety commitments are tracked obligations.

Rivals plan weekly at Standard settings using public facts, their private company state, and bounded market estimates. The explicit difficulty profile can change their bounded review interval; the setup preview discloses the spacing. In the scripted introduction, difficulty scales the initial reaction delay instead, preserving warning lead times and actual paid work. Score a short list of feasible strategic actions (specialize, research, price, hire, finance, expand, publish, partner) against their objectives and available budget. Use a stable tie-break plus their seeded preference variation. They may make mistakes, but cannot ignore accounting or research prerequisites. Catch-up help, if enabled on a forgiving difficulty, is disclosed in setup and never provides hidden unlimited money.

Heavyweight pressure accumulates from sustained substitution of its existing revenue, publicly demonstrated useful capability, adoption, and financed rival expansion. Thresholds trigger watch → mobilize after a visible warning period of at least 90 days. Mobilization reserves a finite internal budget, hires staff, and acquires capacity before delivering results. Exit/consolidation requires pressure falling below a lower threshold for two review periods; hysteresis avoids monthly state flicker. Exact pressure components and the player's knowledge of them are visible in market intelligence.

## 7. Politics, macro effects, and hazards

Each US legislative chamber stores seat counts by coalition; coalition issue positions, governing relationships, and constituency pressures determine proposal support. House elections occur every two years, Senate thirds every two years, presidency every four years starting with the 2016 cycle. Election outcomes aggregate economic conditions, incumbent performance, issue salience, lawful campaigning, and bounded uncertainty. Geography supplies state-level opinions and representatives without individual voter records.

Laws are typed modifiers with jurisdiction, affected activities, thresholds, exemptions, implementation date, enforcement capacity, and review state. A company sees why a product is ineligible and what can restore eligibility. A dispute can suspend a specific enforcement action only where its rules allow; filing a challenge does not suspend every law.

Regions update monthly from adoption and actual deployment outcomes: productivity, labor displacement/retraining, public-service access, energy strain, concentration, information integrity, and social trust. Policy and investment change adaptation rates with lags. Do not award world welfare directly from dollars spent. Calculate funded capacity, delivered services, access, and resulting welfare. Distinguish a private campaign contribution from an operating public-service investment.

Century regional profiles can add a capped, durable gain to the original
welfare baseline. Each region earns it only from settled public service,
overall useful delivery and independent quality that meet the published
thresholds. The usual observation lag applies. Below-threshold months erode
part of the accumulated gain. The charter reports coverage, quality, growth,
decay and the cap; approved spending alone cannot move this measure.

The five utopia indices (basic needs, health, civic agency, ecological resilience, shared prosperity) are bounded 0–100, with published component weights. Initial v1 weights are equal within each index: basic needs = food/housing/essential-service access; health = effective care access/prevention; agency = institutional checks/civil participation/information integrity; resilience = emissions trend/resource resilience/adaptation capacity; prosperity = median real income opportunity/transition support/distribution. Components are game measures, not official international statistics. Population-weighted global values and each region's minimum are shown separately.

Governance authority is also explicit: enforceable participation in standards/procurement, control of critical infrastructure with binding decision rights, and institutional delegation combine into regional soft authority; control over abstract coercive institutions produces hard authority and resistance. Ordinary revenue, media attention, or market share is insufficient. Soft authority decays when agreements expire or institutions revoke mandates. Hard authority requires continuing capacity and can trigger coordinated resistance, service disruption, sanctions, staff refusal, and loss of legitimacy. No tactical weapons, real coercion instructions, or real people are depicted.

Site hazards use region-appropriate authored probabilities, seasonality, maintenance, and preparedness. Insurance changes financial loss, not physical uptime. Backup copies and capacity replication have independent effects. Event explanations separate unavoidable exposure, mitigation performed, and an actually available alternative.

## 8. AGI/ASI validation and objective accounting

Game AGI requires all six domain scores ≥75, held-out transfer ≥70, reliability ≥85, and autonomous workflow competence ≥80, demonstrated in three independent assessment windows spanning at least 90 days. A bundle of cooperating models may qualify if evaluated as one deployed system; its resource needs and failure modes are combined, not ignored.

Game ASI requires all domains ≥90, novel science/engineering ≥95, and three independently replicated improvements from its research process across at least 180 days. Research, physical verification, and integration costs still apply. These thresholds are fictional labels. Control quality is reported alongside them but is not an ASI-race prerequisite. Models cannot self-certify, and assessment windows must use held-out tasks.

Monopoly accumulates delivered useful work by region/company over the final 60 settled calendar months (January 2109–December 2113). Its global/regional thresholds use aggregate task sums, not an every-month threshold: a single weak month is not automatically fatal. Regional shares use their own denominators. If a region has no delivered tasks, it does not satisfy the regional threshold; financing a token shell deployment does not qualify without meaningful demand. Domination requires qualifying regional authority throughout those final 60 months, covering ≥80% population and output on every month-end check. Utopia requires all thresholds at each of the final 120 month-end checks (January 2104–December 2113). Store monthly sums, exact check dates and violation counters rather than only checking the final day.

Track objective feasibility separately from ordinary progress. An irreversible final-window breach for Domination/Utopia immediately shows the failed check/threshold and auto-pauses by default. Offer an immediate postmortem, continued scored play for other distinctions, or custom sandbox continuation; never require years of play to see an already determined primary-objective loss. Monopoly gets an impossibility alert only when accumulated totals and defensible remaining bounds prove it, not from a bad monthly share or a pessimistic forecast. Preserve breach/acknowledgment state across reload. Keep the century deadline and strict windows in the default rules; rolling early victory/grace months would change objective balance and require a separate decision/playtest.

Profit uses **founder-legacy wealth**: the retained founder trust's ownership fraction times terminal company equity value, plus its retained personal cash/investments, minus its personal liabilities, already stored in constant 2014 cents. Company cash is already inside equity value and is not added again. Founder succession preserves this trust; the new CEO does not inherit a free second starting stake. Terminal equity value uses net realizable assets plus a capped multiple of audited trailing positive operating profit, less debt, floored at zero; the multiple and asset haircuts are equal for the player and rivals. One-off self-deals cannot manufacture audited profit. A tie shares the profit win.

Survival means positive operating status, liabilities serviced or within an agreed workout, at least one active team, and either paid useful work or a funded active research program in the final year. A dormant cash shell does not qualify. A warning starts the 30-day insolvency workout; rescue, asset sale, negotiated restructuring, or an approved merger can resolve it.

Objective completion/failure checks occur after the day's committed transactions. ASI simultaneous completion shares a win; no entity-ID advantage. Save/load preserves all objective windows, deadlines, and prior violations.

## 9. Balancing and long campaigns

Use three baseline strategies in automated scenarios: careful specialist, publishing ecosystem builder, and capital-intensive frontier lab. They should have distinct advantages and viable survival paths. Add adversarial policies for cash hoarding, infinite dilution, zero-price share capture, self-dealing, perpetual contract rerolls, infinite research automation, and domination through ordinary advertising. Fix causes instead of silently penalizing a named strategy.

Benchmark initial worlds up to 32 AI institutions, 51 domestic jurisdictions, eight international profiles, 100 player product/version records, and 50 player sites. Aggregate ordinary employees and customers. Bound transient event queues, compact logs into monthly summaries, retire obsolete offers, and preserve important historical facts. Sandbox has no designed ending; test at least 500 years and report the tested range rather than claiming unbounded memory.

Century pacing includes a bounded research queue (revalidate prerequisites/budget when each item starts), optional standing policies for repeatable research, and mature-product refresh within explicit cash/staff/idle-compute ceilings. Refresh uses licensed eligible data, creates a candidate version, and must pass the policy's held-out evaluation/rollout gate; it cannot silently broaden rights, release weights or bypass serving commitments. Notifications distinguish required decision/auto-pause, actionable inbox with an authored simulated deadline/reminder, and informational digest. Do not invent a deadline for routine news or repeatedly interrupt fastest speed. M3 proves basic routing/policy bounds; M7/M8 qualify mature-year workload.

Balance changes require a named content version, recorded seeds, financial reconciliation, and comparable policy runs. Human playtests judge clarity, variety, pacing, and recovery. Winning all seeded bot runs is not evidence that the game is fun or that its view of AI is correct.
