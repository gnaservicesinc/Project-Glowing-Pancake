# Simulation and balance specification

Baseline 1.0 · 2026-11-03 · Implementation contract, with provisional balance constants

Read alongside [the game design](GAME_DESIGN.md). The simulation models fictional companies and simplified world systems. Its formulas support interesting, understandable choices; they are not predictions of actual AI development, investment returns, elections, or catastrophic risk.

## 1. Time, units, and reproducibility

One authoritative step is a day. A state dated `t` describes the opening of that day; stepping applies activity over `[t, t+1)` and produces the opening state for the next day. The campaign stops when that date reaches 2114-01-01. Weekly reviews occur every seventh day from the campaign epoch; monthly and annual settlements follow the Gregorian calendar. All deadlines name a date and a phase, avoiding ambiguous inclusive end dates.

| Quantity | Stored unit | Display |
| --- | --- | --- |
| Money | Signed checked 64-bit integer cents | Localized dollars; optional constant-2014-dollar view |
| Population/requests/tasks | Checked integer counts or explicitly named thousand-unit counts | Counts with unit labels |
| Capacity | Integer milli-compute-units per day | CU/day; a scenario-normalized reference, not advertised FLOPS |
| Energy | Integer watt-hours | kWh/MWh |
| Power | Integer watts | kW/MW |
| Skill, confidence, trust, quality | Integer 0–10,000 | 0–100 with labels and uncertainty where appropriate |
| Probabilities | Integer parts per million | Percent or verbal band |
| Shares and rates | Basis points unless a schema names a different scale | Percent |
| Time/duration | Integer ordinal day and integer days | Date and duration |

Money arithmetic checks overflow; intermediate products use a portable checked multiply/divide helper. Specify round-half-up for nonnegative amounts; post signed values as a sign plus a nonnegative rounded magnitude. Allocate rounding residues by stable entity ID so totals reconcile. No currency conversion by binary floating-point in the authoritative state.

Use a documented integer PRNG algorithm with independent streams for research, rivals, markets, geography, politics, and narrative events. Save each stream state. Entity iteration, event ordering, and tie-breaking are explicit. Do not use `std::hash`, library-dependent random distributions, current time, wall-clock duration, or unordered container iteration to decide outcomes. Integer lookup tables or rational approximations replace platform-dependent transcendental functions. Rendering and forecast previews never consume authoritative randomness.

Reproducibility means identical simulation version, content hashes, seed, and command sequence give the same state hashes on every supported architecture. New balancing content can change outcomes only under a new content version. A later version can migrate a save without promising identical continuation under changed rules.

## 2. Daily transaction order

1. Validate and apply queued player/AI commands against the opening state. Reserve money, staff, and capacity; refuse the entire command if a prerequisite fails.
2. Apply scheduled deliveries, effective regulations, contract transitions, and exogenous conditions. Resolve shared resource allocation from a common snapshot rather than company iteration order.
3. Assign available staff and compute according to validated policies. Include serving commitments, maintenance, and outages.
4. Advance research, data processing, training, and evaluation. Newly completed models can be deployed by a command on the next day, not retroactively.
5. Allocate contract demand and discretionary demand simultaneously; serve capacity-limited useful work; book revenue, refunds, service costs, and contractual penalties.
6. Accrue payroll, power, debt, taxes, and other obligations. Settle amounts on their due dates; run liquidity warnings and workouts.
7. Perform due weekly/monthly/annual updates: rival strategy, adoption, macro effects, politics, staffing reviews, and hazard sampling.
8. Generate event facts and consequence explanations, update objective accumulators, increment the date, and publish an immutable UI snapshot. Stop at an auto-pause boundary before processing another day.

AI decisions are commands using the same validation rules. A decision requiring a player response pauses before its deadline; unresolved optional correspondence follows the disclosed default. Async jobs may prepare calculations, but only the simulation thread commits results in canonical order.

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

The slice must be survivable by a careful specialist strategy without a new funding round, and a financing strategy must also work. This is an acceptance target to establish by simulation/playtest, not an assertion that the initial values already prove a three-year balance. A smaller prototype, staged customer payment, staff changes, and profitable contract expansion provide recovery routes.

## 4. Ledgers, capacity, and markets

### Financial rules

Every posting has day, debit account, credit account, amount, cause, and owning entity. The accounts model includes cash, receivables, assets, accumulated depreciation, payables, debt, equity, revenue, and expenses. Financial reports derive from postings; UI values cannot edit balances. Cash reservations are commitments, not recognized expenses until the relevant transaction occurs. Credits, deposits, and equity proceeds are not product revenue.

Nominal prices use a world price index, initialized to 10,000 in 2014. Reports and the profit objective convert with the saved index. Interest and monthly recurring payments accrue using specified day-count rules; the first implementation uses actual days in the contract's calendar month. Taxes are fictional policy parameters on explicit taxable bases, not reproductions of any jurisdiction's current tax code. Carries, relief, and incentives have caps, eligibility, and expiration.

All equity allocations sum to 10,000 basis points. Dilution changes ownership and does not automatically change seat allocation. A merger conserves assets, debt, obligations, licenses, and control rights; acquired closed products do not instantly become compatible. The first version supports cash/equity purchases and straightforward mergers, not arbitrary corporate derivatives.

### Shared physical capacity

Daily effective capacity is the minimum allowed by installed equipment, interconnect/memory requirements, staffing, grid/lease allocation, and availability. A project reserves a feasible slice of that capacity. Inference has a configurable protected minimum; overcommitment either queues work, slows training, or refuses a new commitment according to policy. The same hardware cannot be counted as cloud and owned capacity simultaneously.

Owned-site energy per day:

`energy_Wh = (active_IT_watts × active_hours + idle_IT_watts × idle_hours) × PUE`

Use fixed-point PUE ≥1, include cooling once, and apply backup power/water/maintenance costs through separate typed inputs. Delivered CU depend on hardware efficiency and utilization, not energy price. Electricity cost is energy in kWh times contracted price; lease prices state whether energy is included. Grid capacity and construction lead times are finite. Curtailment reduces work performed and associated energy rather than charging for imaginary full utilization.

### Customer allocation

Each region/segment has potential useful-task demand, a budget, incumbent workflows, and requirements. Contract obligations reserve demand first. Discretionary demand compares eligible products and an outside option (using no AI or keeping an existing non-AI process).

For eligible product `p` in segment `s`, compute a nonnegative integer utility from task fit, reliability, latency, trust, support, switching cost, visibility, and price relative to the segment budget. All coefficients are data. Weight allocation by the square of positive utility above the reservation threshold. Include the outside-option weight in the denominator. Enforce willingness-to-pay and legal/technical eligibility before allocation.

Allocate from a common snapshot; cap by customer budget, contract terms, and product serving capacity; redistribute unserved demand in stable rounds until there is no remaining eligible capacity or demand. Preserve total tasks and money. Unserved work returns to competitors or the outside option; it does not create revenue. Advertising affects awareness with diminishing returns, not capability or guaranteed purchases.

Useful-task units are scenario-defined equivalents by task family. They measure successfully completed work, not raw tokens or model size. The denominator includes all active vendors in all regions; the outside option is reported separately and does not count as delivered AI work. Publishing a free model produces no delivered share merely from downloads. Deployed open/on-premise systems generate modeled work attributed once to their primary vendor, with component suppliers excluded from the same market-share numerator. Closed and open distribution use the same attribution rule.

## 5. Models, data, and research

### Resource-to-capability model

Let `sat(x,k) = floor(10,000 × x / (x+k))`, with nonnegative quantities and positive tuning constant `k`. It provides diminishing returns without logarithms. Architecture/recipe definitions specify a domain baseline, a maximum training gain, compute/data scales, memory/interconnect constraints, and serving costs.

For domain `j`:

`effective_data_j = sum(usable_unique_units_i × quality_i × relevance_ij × coverage_ij)`

`bottleneck_j = min(sat(consumed_CU, compute_scale_j), sat(effective_data_j, data_scale_j))`

`capability_j = min(research_ceiling_j, architecture_baseline_j + training_gain_j × bottleneck_j × recipe_fit_j)`

All quality/fit factors above are basis-point values; divide once per multiplication using the specified checked arithmetic. Research ceilings, recipe fit, and gain coefficients are scenario data. The formula expresses complementary bottlenecks; it is deliberately not an empirical scaling law. Actual research motivates the distinction between compute, data, and parameter choices, not these game coefficients. See [research sources](RESEARCH.md).

Rights and consent affect cost, availability, contracts, litigation, trust, and governance; they are not hidden direct deductions from mathematical training capability. Quality and rights can correlate in an authored offer but remain independent attributes. Data contamination affects reported evaluation, not genuine capability. Synthetic-data quality depends on parent capability, diversity, filtering, and external verification, with increasing duplicate/coverage limits.

A run's score is computed from its recipe and accumulated effective resources. Continuing a checkpoint updates that accumulation; repeatedly cloning/restarting a checkpoint does not add the previous score as a new bonus. Transfer learning may preserve a documented baseline and reduce required work, subject to a recipe-specific ceiling. Specialist fine-tuning can reduce performance elsewhere. Checkpoint lineage and data rights follow every derived model.

### Evaluation and deployment

Maintain true simulation capability, measured estimates, coverage, uncertainty, and observed field performance as separate fields. The player sees evidence and an interval; the simulator needs true values for customer outcomes. More suitable independent tests narrow uncertainty. Buying an audit cannot directly raise the underlying score.

Reliability, resistance to distribution shift, controllability, operational monitoring, and permission boundaries are separate axes. Deployment controls reduce specific failure modes and may change cost, latency, or eligible markets. For example: human approval reduces unsupported autonomous actions and throughput; stronger evaluation improves estimates and catches defects before rollout; monitoring shortens response time after a detectable incident. Do not reduce all controls to an additive safety percentage.

Incident probability is a scenario-authored integer lookup over deployment scale, exposure, autonomy, task severity, capability-control mismatch, and relevant mitigations. Monthly sampling uses the risk stream; schedule the incident within the next period. Independent local outages use site hazard tables. Rare world-ending chains require explicit precursor conditions and a documented sequence of escalations; no unavoidable random nuclear game-over roll. Numbers are fictional, with visible risk bands rather than fake precision about actual AI risk.

### Research implementation

A research definition contains prerequisites (`all_of`, `any_of`), field, required knowledge, staff-days, experiment CU, cash, maturity, outputs, failure-learning credit, and allowed publication choices. Progress accrues from qualified effective staff-days and completed experiments. Changing teams causes a bounded handover cost. Research hazards depend on project/knowledge state, not how often the player presses a button.

Initial six nodes: efficient image features; sequence modeling; data deduplication; held-out evaluation; efficient serving; reproducible experiment tooling. Later groups introduce attention-based architectures, multimodality, preference adaptation, tool use, retrieval, verification, mechanistic analysis, robust deployment, advanced hardware/software co-design, and speculative broad research systems. Nodes create capabilities and constraints for recipes/products. The content team expands the graph only after the slice loop works.

## 6. Organization and opponents

Teams have skill capacity, salaries, hiring pipeline, workload, morale, and knowledge retention. Effective staff-days are available people-days times task fit and a bounded morale/coordination factor. Adding people has onboarding and coordination costs. Burnout accumulates with sustained workload and recovers through time and working conditions; bonuses alone do not erase it instantly.

The current executive's base, appointments, travel days and delegated authority are explicit calendar state. Trips reserve executive availability and expense without disabling remote company management. On-site inspection improves only the evidence actually observable there; a meeting can affect a relationship without guaranteeing a deal. Scheduling must detect overlapping commitments and allow a remote/delegated alternative where the appointment permits it.

Executive confidence is a weighted record of delivered results, compensation, autonomy, values, and promises. A major dispute is a state machine: concern → formal recommendation → negotiation/deadline → resolved, departure, board motion, or disclosure. A disclosure requires knowledge of a concrete issue and a failed response path; it is not simply a personality die roll. Promised appointments, publication rights, and safety commitments are tracked obligations.

Rivals plan weekly using public facts, their private company state, and bounded market estimates. Score a short list of feasible strategic actions (specialize, research, price, hire, finance, expand, publish, partner) against their objectives and available budget. Use a stable tie-break plus their seeded preference variation. They may make mistakes, but cannot ignore accounting or research prerequisites. Catch-up help, if enabled on a forgiving difficulty, is disclosed in setup and never provides hidden unlimited money.

Heavyweight pressure accumulates from sustained substitution of its existing revenue, publicly demonstrated useful capability, adoption, and financed rival expansion. Thresholds trigger watch → mobilize after a visible warning period of at least 90 days. Mobilization reserves a finite internal budget, hires staff, and acquires capacity before delivering results. Exit/consolidation requires pressure falling below a lower threshold for two review periods; hysteresis avoids monthly state flicker. Exact pressure components and the player's knowledge of them are visible in market intelligence.

## 7. Politics, macro effects, and hazards

Each US legislative chamber stores seat counts by coalition; coalition issue positions, governing relationships, and constituency pressures determine proposal support. House elections occur every two years, Senate thirds every two years, presidency every four years starting with the 2016 cycle. Election outcomes aggregate economic conditions, incumbent performance, issue salience, lawful campaigning, and bounded uncertainty. Geography supplies state-level opinions and representatives without individual voter records.

Laws are typed modifiers with jurisdiction, affected activities, thresholds, exemptions, implementation date, enforcement capacity, and review state. A company sees why a product is ineligible and what can restore eligibility. A dispute can suspend a specific enforcement action only where its rules allow; filing a challenge does not suspend every law.

Regions update monthly from adoption and actual deployment outcomes: productivity, labor displacement/retraining, public-service access, energy strain, concentration, information integrity, and social trust. Policy and investment change adaptation rates with lags. Do not award world welfare directly from dollars spent. Calculate funded capacity, delivered services, access, and resulting welfare. Distinguish a private campaign contribution from an operating public-service investment.

The five utopia indices (basic needs, health, civic agency, ecological resilience, shared prosperity) are bounded 0–100, with published component weights. Initial v1 weights are equal within each index: basic needs = food/housing/essential-service access; health = effective care access/prevention; agency = institutional checks/civil participation/information integrity; resilience = emissions trend/resource resilience/adaptation capacity; prosperity = median real income opportunity/transition support/distribution. Components are game measures, not official international statistics. Population-weighted global values and each region's minimum are shown separately.

Governance authority is also explicit: enforceable participation in standards/procurement, control of critical infrastructure with binding decision rights, and institutional delegation combine into regional soft authority; control over abstract coercive institutions produces hard authority and resistance. Ordinary revenue, media attention, or market share is insufficient. Soft authority decays when agreements expire or institutions revoke mandates. Hard authority requires continuing capacity and can trigger coordinated resistance, service disruption, sanctions, staff refusal, and loss of legitimacy. No tactical weapons, real coercion instructions, or real people are depicted.

Site hazards use region-appropriate authored probabilities, seasonality, maintenance, and preparedness. Insurance changes financial loss, not physical uptime. Backup copies and capacity replication have independent effects. Event explanations separate unavoidable exposure, mitigation performed, and an actually available alternative.

## 8. AGI/ASI validation and objective accounting

Game AGI requires all six domain scores ≥75, held-out transfer ≥70, reliability ≥85, and autonomous workflow competence ≥80, demonstrated in three independent assessment windows spanning at least 90 days. A bundle of cooperating models may qualify if evaluated as one deployed system; its resource needs and failure modes are combined, not ignored.

Game ASI requires all domains ≥90, novel science/engineering ≥95, and three independently replicated improvements from its research process across at least 180 days. Research, physical verification, and integration costs still apply. These thresholds are fictional labels. Control quality is reported alongside them but is not an ASI-race prerequisite. Models cannot self-certify, and assessment windows must use held-out tasks.

Monopoly accumulates delivered useful work by region/company over the rolling final 60 calendar months. Regional shares use their own denominators. If a region has no delivered tasks, it does not satisfy the regional threshold; financing a token shell deployment does not qualify without meaningful demand. Domination requires qualifying regional authority throughout the final 60 months, covering ≥80% population and output on every monthly check. Utopia requires all thresholds on each of the final 120 monthly checks. Store rolling sums and violation counters rather than only checking the final day.

Profit uses **founder-legacy wealth**: the retained founder trust's ownership fraction times terminal company equity value, plus its retained personal cash/investments, minus its personal liabilities, all in constant 2014 dollars. Company cash is already inside equity value and is not added again. Founder succession preserves this trust; the new CEO does not inherit a free second starting stake. Terminal equity value uses net realizable assets plus a capped multiple of audited trailing positive operating profit, less debt, floored at zero; the multiple and asset haircuts are equal for the player and rivals. One-off self-deals cannot manufacture audited profit. A tie shares the profit win.

Survival means positive operating status, liabilities serviced or within an agreed workout, at least one active team, and either paid useful work or a funded active research program in the final year. A dormant cash shell does not qualify. A warning starts the 30-day insolvency workout; rescue, asset sale, negotiated restructuring, or an approved merger can resolve it.

Objective completion/failure checks occur after the day's committed transactions. ASI simultaneous completion shares a win; no entity-ID advantage. Save/load preserves all objective windows, deadlines, and prior violations.

## 9. Balancing and long campaigns

Use three baseline strategies in automated scenarios: careful specialist, publishing ecosystem builder, and capital-intensive frontier lab. They should have distinct advantages and viable survival paths. Add adversarial policies for cash hoarding, infinite dilution, zero-price share capture, self-dealing, perpetual contract rerolls, infinite research automation, and domination through ordinary advertising. Fix causes instead of silently penalizing a named strategy.

Benchmark initial worlds up to 32 AI institutions, 51 domestic jurisdictions, eight international profiles, 100 player product/version records, and 50 player sites. Aggregate ordinary employees and customers. Bound transient event queues, compact logs into monthly summaries, retire obsolete offers, and preserve important historical facts. Sandbox has no designed ending; test at least 500 years and report the tested range rather than claiming unbounded memory.

Balance changes require a named content version, recorded seeds, financial reconciliation, and comparable policy runs. Human playtests judge clarity, variety, pacing, and recovery. Winning all seeded bot runs is not evidence that the game is fun or that its view of AI is correct.
