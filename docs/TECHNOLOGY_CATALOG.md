# Candidate technology catalog

Design baseline 1.3 · 2026-11-07 · 140 candidates across ten domains, plus ten refinement families

This is a reviewed seed catalog for [the technology design](TECHNOLOGY.md),
not finalized content, an implemented schema or a commitment to ship 140 nodes.
Each selected entry must add a meaningful decision, use an implemented effect,
receive balance values and pass authoring validation. Merge entries that merely
repeat the same choice. The existing 60-topic prototype remains unchanged.

**Kinds:** B = baseline public knowledge/practice available in 2015; E = an
established concept or authored combination of established methods; S = a
speculative capability or future institutional application. S? additionally
requires an explicitly enabled optional future scenario. E does not mean this
exact product was historically deployed at the listed date. Individual historic
dates need sources before production; checked anchors are in TECHNOLOGY.md.

**EEY offset** is an illustrative frontier target within the era bands, not a
guaranteed unlock or scientific prediction. `+` means AND, `OR` means OR, with
parentheses explicit. `—` means no catalog knowledge prerequisite. IDs are
stable planning identifiers. Installed tools, qualified staff, permissions,
partner access and paid implementation remain additional requirements, even
when a baseline prerequisite is widely known. A dependency is not ownership
of a factory, institute or supplier. Purchased services can meet operating
requirements while knowledge remains with their provider.

## Domains, subareas and work requirements

| Domain | Subareas / branches | Typical staff and tools; alternative access |
| --- | --- | --- |
| ML | Predictive, sequence, vision, multimodal, agents, research assistance | Qualified ML/software staff; leased training, serving and evaluation capacity |
| Data | Rights, curation, retrieval, language coverage, memory, simulation | Data engineering/domain reviewers; licensed corpora, versioning and test sets |
| Mathematics | Optimization, numerical methods, statistics, allocation, verification | Applied math/systems staff; reproducible experiments, compatible kernels and proof tools |
| Compute | Accelerators, interconnects, serving, scheduling, resilience, emerging substrates | Systems/operations staff; lease or buy supported vendor hardware and services |
| Materials | Packaging, cooling, storage, sourcing, lifecycle and physical limits | Supplier/materials expertise; commission partners instead of owning fabrication |
| Biology | Beneficial domain tools, clinical evidence, screening, neuroscience and optional substrates | Domain professionals and independent labs; no unqualified wet-lab execution by a software startup |
| Social science | Interaction, organizations, markets, transitions and institutions | Researchers, user studies, economists and local partners; bounded aggregate populations |
| Ethics/control | Consent, disclosure, evaluation scope, oversight, pluralism and governance | Domain/control specialists and stakeholder review; evaluations and incident records |
| Business | Entity/charter, contracts, grants, metering, licensing, finance and continuity | Legal/accounting/product/operations capability; paid professional services available |
| Robotics | Perception, control, safety, integration, maintenance and fleet lifecycle | Licensed partner hardware, simulation, test facilities and qualified integrators |

Baseline entries describe what already exists; they are not mandatory research
chores for incorporation, ordinary contracts, calculus, hiring or subscriptions.
An advanced implementation project can improve those operations. A research
lab does not have to study all domains to use the market or remain viable.

## 1. Machine learning and core AI

| ID | Candidate | Kind / EEY | Knowledge prerequisites | Option unlocked and remaining constraint |
| --- | --- | --- | --- | --- |
| ML01 | Recurrent sequence models and attention | B / 0 | MA01 + MA02 | Translation/basic language recipes; sequential throughput and task coverage limit scale |
| ML02 | Trees and structured prediction | B / 0 | MA03 | Tabular specialist products; good records matter more than chatbot prestige |
| ML03 | Convolutional vision features | B / 0 | MA01 + MA02 | Inspection/image recognition recipes; independent subgroup tests still needed |
| ML04 | Transformer sequence architecture | E / 2 | (ML01 OR ML03) + MA01 + MA02 | Parallel sequence training family; not an instant conversational product |
| ML05 | Instruction adaptation | E / 7 | (ML01 OR ML04) + DA01 + DA02 + SO05 | Following instructions within trained scope; annotation quality and serving costs |
| ML06 | Preference optimization alternatives | E / 8 | ML05 + DA02 + MA03 | DPO or reward-model variant choices; evaluation and preference coverage remain |
| ML07 | Multimodal representation and generation | E / 10 | ML04 + ML03 + DA08 | Combined text/image/audio product families; modality data, rights and serving needs differ |
| ML08 | Tools and supervised workflows | E / 9 | ML05 + ET03 + MA04 | Permission-bounded tool/agent recipes; verified task completion and oversight |
| ML09 | Sparse expert routing | E / 10 | ML04 + MA08 + HW03 | Active-parameter scaling choice; routing, memory and communication tradeoffs |
| ML10 | Validated continual adaptation | S / 22 | ML04 + DA09 + ET08 | Update approved knowledge with retention tests; drift/rollback/tenant separation |
| ML11 | Research-assisting systems | S / 38 | ML08 + MA12 + DA11 + ET10 | Budgeted candidate experiments and software assistance; independent replication |
| ML12 | Adaptive architecture co-design | S? / 50 | ML11 + HW08 + MA11 | Controlled architecture exploration; physical validation and plateau alternative |
| ML13 | Diffusion/image generation recipes | E / 7 | ML03 + DA01 + DA02 | Image-generation product options; rights, compute and fidelity evaluation |
| ML14 | Neural speech recognition | E / 8 | ML01 + DA08 + DA02 | Transcription services; accents, noise, consent and latency |
| ML15 | Neural speech synthesis | E / 4 | ML01 + DA01 + DA02 | TTS product family; licensed voices, accessibility and intelligibility |
| ML16 | Interactive audio systems | E / 10 | ML14 + ML15 + SO05 + ET07 | Real-time voice interfaces; turn-taking, latency and abuse/consent boundaries |
| ML17 | Music generation systems | E / 9 | ML01 + DA01 + DA02 | Fictional music-service market; provenance and task-specific quality; no generated game soundtrack |
| ML18 | Sound-effect generation systems | E / 10 | ML13 + DA01 + DA02 | Fictional sound-design tools; synchronization, rights and evaluation |
| ML19 | Video and predictive world models | E / 10 | ML13 + ML07 + DA02 | Video/simulation recipes; temporal consistency and high serving/training costs |
| ML20 | Geometry and constrained CAD assistance | S / 24 | ML07 + MA04 + RO04 | Design-assistance products; tolerances, manufacturability and independent checks |
| ML21 | Code completion and repository context | E / 6 | ML04 + DA01 + MA04 | Coding assistance; eligible code, test quality and customer integration |
| ML22 | Verified software-maintenance agents | E / 10 | ML21 + ML08 + ET08 | Bounded patch workflows; sandboxing, tests and human release approval |
| ML23 | Reinforcement learning and planning | E / 2 | ML02 + MA02 + DA02 | Alternative task/planning recipes; sample efficiency and environment validity |
| ML24 | Scientific surrogate models | S / 24 | ML02 + MA11 + BI02 | Faster candidate screening in defined domains; external validation and uncertainty |

## 2. Data engineering, linguistics and information science

| ID | Candidate | Kind / EEY | Knowledge prerequisites | Option unlocked and remaining constraint |
| --- | --- | --- | --- | --- |
| DA01 | Licensed data and provenance ledger | B / 0 | — | Track actual use rights and renewal scope; public availability alone grants no rights |
| DA02 | Held-out task evaluation | B / 0 | MA03 | Independent test split and coverage reporting; evidence work does not raise true capability |
| DA03 | Deduplication and dataset versioning | B / 0 | DA01 | Reduce duplication and preserve lineage; coverage can fall if filtering is excessive |
| DA04 | Lexical retrieval and document indexing | B / 0 | DA01 | Low-cost search/knowledge service baseline; matching and language limits |
| DA05 | Dense retrieval and grounded generation | E / 7 | DA04 + ML04 + DA02 | Retrieval-backed answers; stale records, permissions and citation errors remain |
| DA06 | Dataset provenance and contamination audits | E / 5 | DA03 + DA02 | Audit ingestion and evaluation overlap; staff/cost versus coverage confidence |
| DA07 | Verified synthetic-data workflows | E / 10 | ML05 + DA06 + MA04 | Filtered derived training data; teacher limits, diversity and external verification |
| DA08 | Multilingual/domain coverage engineering | E / 8 | DA03 + SO01 | Better coverage mixtures; qualified local reviewers and rights required |
| DA09 | Permission-scoped persistent memory | S / 16 | DA05 + ET01 + BU06 | Selective organizational memory; deletion, leakage, retention and maintenance |
| DA10 | Adaptive data curricula | S / 21 | DA03 + MA09 + ML05 | Spend work on measured weaknesses; protect independent evaluation |
| DA11 | Validated task simulators | S / 32 | DA07 + MA11 + RO04 | More reproducible experiments; simulation fidelity does not prove real-world transfer |
| DA12 | Long-horizon knowledge transfer | S / 50 | DA09 + DA10 + DA11 | Retain evidence across replacements; transfer tests and obsolete rights review |
| DA13 | Subword/byte representation engineering | B / 0 | ML01 + DA01 | Existing alternative text encodings; implementation choices affect language coverage and memory/sequence tradeoffs |

## 3. Mathematics and theoretical computer science

| ID | Candidate | Kind / EEY | Knowledge prerequisites | Option unlocked and remaining constraint |
| --- | --- | --- | --- | --- |
| MA01 | Numerical linear algebra | B / 0 | — | Baseline matrix/sparse operations; finite precision and compatible hardware |
| MA02 | Gradient optimization and experiment design | B / 0 | MA01 | Baseline training/optimizer options; not guaranteed convergence |
| MA03 | Statistics and uncertainty | B / 0 | — | Sampling, confidence and calibration foundations; evidence quality matters |
| MA04 | Formal specifications and bounded verification | B / 0 | — | Check narrow software properties and task verifiers; no proof of general safety |
| MA05 | Low-rank adaptation | E / 6 | ML04 + MA01 | Cheaper task adaptation; pretrained model/data permissions and performance limits |
| MA06 | Mixed-precision training | E / 2 | MA02 + HW02 | Memory/throughput tradeoffs with compatible numerical paths; stability testing |
| MA07 | Quantized deployment | E / 7 | MA01 + HW05 + DA02 | Smaller/faster selected deployments; hardware-specific kernels and re-evaluation |
| MA08 | IO-aware attention implementations | E / 7 | ML04 + MA01 + HW02 | More efficient compatible attention execution; no universal benchmark multiplier |
| MA09 | Calibration and selective abstention | E / 8 | MA03 + DA02 | Better confidence behavior/deferral; coverage and refusal tradeoff |
| MA10 | Resource-constrained experimental allocation | S / 16 | MA03 + HW08 | Better scheduling/selection under uncertainty; real experiments remain necessary |
| MA11 | Causal and counterfactual task models | S / 24 | MA03 + DA06 | Test intervention hypotheses; assumptions, data and domain verification |
| MA12 | Compositional verification toolchains | S / 36 | MA04 + ML08 + ET08 | Broader verified software workflows; proof scope, trusted tools and review cost |
| MA13 | Matrix-aware optimizer variants | E / 10 | MA02 + HW03 + DA02 | Alternative eligible training optimizers; stability, memory and workload-specific validation |

## 4. Compute, physics and infrastructure

| ID | Candidate | Kind / EEY | Knowledge prerequisites | Option unlocked and remaining constraint |
| --- | --- | --- | --- | --- |
| HW01 | Leased compute operations | B / 0 | — | Obtain finite compatible training/serving capacity; price, term and supplier risk |
| HW02 | Programmable accelerator tooling | B / 0 | MA01 | Workstation/cluster recipes; memory, power and access are separate resources |
| HW03 | Distributed networking and storage | B / 0 | HW02 | Multi-machine work; synchronization, bandwidth and failure overhead |
| HW04 | Backups, observability and resilience | B / 0 | HW01 | Detect/recover specific service failures; replication consumes paid capacity |
| HW05 | Efficient inference serving | E / 3 | HW02 + HW04 | Batching/caching and service isolation; latency and availability tradeoffs |
| HW06 | High-density rack integration | E / 9 | HW03 + MT05 | Procure denser compatible families; grid, cooling, capital and delivery gates |
| HW07 | Validated liquid-cooling deployment | S / 13 | HW06 + MT06 + MT07 | Operate denser sites; thermal maintenance, water/material and power limits |
| HW08 | Shared-cluster scheduling | S / 15 | HW03 + HW05 | Improve measured utilization without double-allocating serving capacity |
| HW09 | Analog inference modules | S? / 32 | HW06 + MT09 + MA07 | Alternative serving family; conversion/noise/precision and vendor availability |
| HW10 | Optical interconnect/accelerator integration | S? / 42 | HW06 + MT05 + HW08 | Selected bandwidth/energy benefits; memory, conversion and fabrication constraints |
| HW11 | Reversible low-dissipation experiments | S? / 75 | HW10 + MT11 | New bounded hardware option; no zero-energy or below-all-physical-limits guarantee |
| HW12 | Fault-tolerant quantum task coprocessors | S? / 50 | MA04 + HW03 + MT09 | Certain validated workloads only; cannot replace all training or cryptography automatically |
| HW13 | Grid/storage and power procurement integration | E / 10 | HW04 + MT04 + BU02 | Diverse power/backup supply agreements; real generation, connection and curtailment limits |
| HW14 | Modular-fission supply partnerships | S? / 35 | HW13 + MT07 + BU10 | Optional firm-power contracts through qualified suppliers; siting, approval, capital and long lead times |
| HW15 | Commercial fusion supply partnerships | S? / 65 | HW13 + MT11 + BU10 | Optional future power family; uncertain availability, finite output and conventional alternatives |

## 5. Chemistry, materials and lifecycle engineering

| ID | Candidate | Kind / EEY | Knowledge prerequisites | Option unlocked and remaining constraint |
| --- | --- | --- | --- | --- |
| MT01 | Vendor material/process specifications | B / 0 | — | Understand supported procurement envelopes; not ownership of semiconductor fabrication |
| MT02 | Thermal fluids and air-cooling basics | B / 0 | — | Safe conventional thermal design; energy, maintenance and site limits |
| MT03 | Archival storage and recovery media | B / 0 | — | Affordable retained evidence/checkpoints; retrieval delay, redundancy and replacement |
| MT04 | Traceable sourcing and disposal | E / 3 | MT01 + DA01 | Disclosed material provenance and end-of-life contracts; actual supplier capacity |
| MT05 | Advanced packaging qualification | E / 8 | MT01 + MA01 | Supported dense-memory/interconnect components; yield, delivery and compatibility |
| MT06 | Qualified dielectric/cooling materials | E / 10 | MT02 + MT04 | Select compatible liquid-cooling supplies; no unsupported chemical or environmental guarantees |
| MT07 | Heat reuse and water stewardship | S / 12 | MT02 + HW04 + SO03 | Site-specific recovery/efficiency projects; local demand, climate and cost |
| MT08 | High-density archival integration | S / 23 | MT03 + DA06 | Lower storage footprint for some records; encode/decode delay and supplier dependence |
| MT09 | Alternative substrate qualification | S? / 30 | MT05 + MA03 | Evaluate analog/post-silicon vendor options; defects, operating envelopes and uncertain viability |
| MT10 | Circular manufacturing partnerships | S / 46 | MT04 + RO08 | Refurbish/recover useful components; material losses and paid processing remain |
| MT11 | Physical-limit accounting | S / 65 | MT05 + MA11 | Evaluate extreme hardware claims; does not grant a magical substrate |
| MT12 | Century resource reserves | S / 95 | MT10 + MT11 + BU12 | Long-term replacement/material contracts; real inventory, obligations and uncertainty |

## 6. Biology, neuroscience and beneficial domain systems

| ID | Candidate | Kind / EEY | Knowledge prerequisites | Option unlocked and remaining constraint |
| --- | --- | --- | --- | --- |
| BI01 | Curated biological/domain records | B / 0 | DA01 | Licensed beneficial-domain data; qualified users and domain coverage |
| BI02 | Domain validation and human review | B / 0 | BI01 + DA02 | Scoped beneficial task evaluation; domain specialists and independent evidence |
| BI03 | Biological/chemical misuse screening | E / 8 | BI01 + BI02 + ET02 | Abstract access/screening controls; false positives and missed cases remain |
| BI04 | Protein/structure research assistance | E / 10 | BI01 + ML03 + DA02 | Beneficial scientific predictions; external experiment time and uncertain validity |
| BI05 | Clinical evidence workflows | S / 14 | BI02 + BU06 + ET07 | Decision support in validated scope; oversight and jurisdiction-specific permission |
| BI06 | Monitored scientific service partnerships | S / 19 | BI03 + BI04 + BU02 | Funded external validation and services; no real hazardous procedures in content |
| BI07 | Neuroscience-inspired adaptation | S / 26 | BI02 + ML04 + MA11 | Candidate retention/learning methods; no claim to replicate consciousness |
| BI08 | Assisted neural-interface services | S? / 28 | BI05 + RO06 + ET01 | Optional partner-led accessibility products; consent, clinical limits and reliability |
| BI09 | Experimental biological compute | S? / 48 | BI07 + MT09 + ET11 | Optional substrate experiments with welfare review; conventional hardware route remains |
| BI10 | Biological/digital welfare evidence | S / 52 | BI02 + ET09 + ET11 | Evaluate uncertainty and oversight needs; not a consciousness detector or guaranteed moral answer |
| BI11 | Ecosystem restoration modeling | S / 72 | BI06 + MA11 + SO10 | Support verified regional restoration projects; field implementation determines outcomes |
| BI12 | Long-term care research networks | S / 95 | BI05 + SO12 + BU12 | Sustained health access/research programs; no effect on the guaranteed founder endpoint |

## 7. Social sciences, economics and institutions

| ID | Candidate | Kind / EEY | Knowledge prerequisites | Option unlocked and remaining constraint |
| --- | --- | --- | --- | --- |
| SO01 | Interviews and user/task studies | B / 0 | — | Discover specific needs and coverage gaps; samples may be unrepresentative |
| SO02 | Team coordination and workload practices | B / 0 | — | Basic people operations; hiring/onboarding and finite attention remain |
| SO03 | Market and price experiments | B / 0 | MA03 | Estimate segment demand; real budgets and uncertainty, no generated customers |
| SO04 | Evidence-based risk communication | E / 6 | SO01 + DA02 | Explain limits/claims to different audiences; candor cannot repair a broken product |
| SO05 | Conversational interaction design | E / 7 | SO01 + SO04 + ML01 | A usable assistant interface, accessible alternatives and uncertainty cues |
| SO06 | Human-assisted workflow adoption | E / 10 | SO02 + ML08 | Measured worker productivity/support; adaptation costs and uneven benefits |
| SO07 | Dependency/sycophancy outcome studies | S / 13 | SO05 + ET06 + DA02 | Detect particular harmful interaction patterns; no universal persuasion firewall |
| SO08 | Sponsored-placement market design | E / 10 | SO03 + BU02 | Auction or direct-sales variants; finite advertisers and conflicts of interest |
| SO09 | Cross-cultural participation | S / 22 | DA08 + ET09 | Better local fit and representation; consultation does not guarantee consensus |
| SO10 | Workforce and public-service transition | S / 27 | SO06 + SO09 + BU09 | Funded regional adjustment partnerships; actual services and lags determine welfare |
| SO11 | Multi-institution standards cooperation | S / 48 | SO09 + ET10 + BU07 | Negotiated standards and verification; local adoption, enforcement and dissent |
| SO12 | Intergenerational institutional studies | S / 95 | SO10 + SO11 + BU11 | Assess continuity/access over decades; contested priorities and incomplete evidence |

## 8. Philosophy, ethics, evaluation and control

| ID | Candidate | Kind / EEY | Knowledge prerequisites | Option unlocked and remaining constraint |
| --- | --- | --- | --- | --- |
| ET01 | Consent, purpose and access boundaries | B / 0 | — | Record what data/tools may be used for; enforcement and rights need implementation |
| ET02 | Independent review and incident procedures | B / 0 | — | Baseline review/escalation; staffing and specific evidence determine effectiveness |
| ET03 | Human interruption and bounded authority | B / 0 | — | Permission/approval controls; human availability and throughput costs |
| ET04 | Honest disclosure and conflict rules | E / 5 | ET01 + ET02 | Disclosed sponsorship/limitations; policies require actual product and staff compliance |
| ET05 | Rule-based model critique | E / 8 | ML05 + ET02 | Constitutional/rule-guided adaptation choices; conflicts and blind spots remain |
| ET06 | Truthfulness and abstention evaluation | E / 8 | MA09 + DA05 | Evaluate grounding and appropriate deferral; does not make all answers true |
| ET07 | Proportional oversight design | E / 10 | ET03 + DA02 + SO04 | Match controls to task severity; different use cases need different measures |
| ET08 | Evaluation scope and deployment change control | E / 10 | ET07 + DA06 | Renew evidence when models/data/use change; operational work and stale evidence |
| ET09 | Value uncertainty and plural review | S / 21 | ET02 + SO01 | Explicit competing priorities; no single ethics meter or prescribed worldview |
| ET10 | Composable bounded-control evidence | S / 33 | ET08 + MA04 + ET09 | Verify particular permissions/interventions; not a proof of universal corrigibility |
| ET11 | Digital/biological welfare governance | S / 45 | ET09 + BI02 | Optional welfare review and safeguards under uncertainty; no automatic personhood finding |
| ET12 | Legacy stewardship charter | S / 95 | ET10 + BU11 + SO12 | Dated institutional commitments beyond the founder; governance can still fail |
| ET13 | Model interpretability experiments | E / 9 | ML04 + MA03 + DA02 | Inspect selected features/causes with evidence; no complete transparency guarantee |
| ET14 | Scoped unlearning and retention checks | E / 10 | ET13 + DA06 + ET01 | Attempt targeted removal while measuring retained tasks; no guaranteed concept excision |

## 9. Business, monetization, law and organization

| ID | Candidate | Kind / EEY | Knowledge prerequisites | Option unlocked and remaining constraint |
| --- | --- | --- | --- | --- |
| BU01 | Corporate/nonprofit formation practice | B / 0 | — | Choose a legal/mission structure; ordinary setup/service, not an invented scientific breakthrough |
| BU02 | Contracts, subscriptions and customer support | B / 0 | — | Ordinary sales, grants of use and service terms; delivery obligations and support costs |
| BU03 | Grants, donations and financial reporting | B / 0 | BU01 | Nonprofit/research funding paths; restrictions and paid eligible activity |
| BU04 | Metered service implementation | E / 5 | BU02 + HW05 + SO03 | API/usage billing variants; metering, support and finite serving demand |
| BU05 | Research evidence and funding communication | E / 6 | DA02 + SO04 + BU03 | Better substantiated pitches; prestige/buzz affect offers, not automatic cash |
| BU06 | Enterprise security/data-handling operations | E / 8 | ET01 + HW04 + MA04 | Private deployments and contractual controls; audits are evidence, not immunity |
| BU07 | Technology licensing operations | E / 8 | BU02 + DA01 + ET04 | Explicit IP/use/royalty terms; actual counterparties, permitted artifacts and payments |
| BU08 | Verified outcome-based contracts | S / 24 | BU04 + MA04 + ML08 | Charge for agreed verified outcomes; attribution, disputes and liability |
| BU09 | Restricted-fund and mission accounting | S / 14 | BU03 + BU02 | Scale compliant mission programs; no restricted cash diverted to founder wealth |
| BU10 | Infrastructure finance partnerships | S / 23 | HW06 + BU02 + BU05 | Budgeted debt/lease/equity offers where legally compatible; covenants and dilution |
| BU11 | Succession and durable governance | S / 42 | BU01 + SO02 + ET09 | Prepared leadership transitions; founder stays alive/in the biography until the campaign boundary |
| BU12 | Endowment/replacement planning | S / 95 | BU09 + BU10 + BU11 | Long-run reserves under the selected legal form; no self-funding perpetual-motion treasury |
| BU13 | Disclosed sponsored consumer tier | E / 11 | ML05 + SO08 + ET04 + BU04 | Auction/direct-sales sponsored placements with measured trust outcomes; fictional ad market only |

## 10. Robotics and cyber-physical systems

| ID | Candidate | Kind / EEY | Knowledge prerequisites | Option unlocked and remaining constraint |
| --- | --- | --- | --- | --- |
| RO01 | Classical control and industrial automation | B / 0 | MA01 | Existing partner robot/control systems; bounded task settings |
| RO02 | Physical interlocks and safety procedures | B / 0 | RO01 + ET03 | Baseline operational protections; inspection, training and residual exposure |
| RO03 | Sensors, localization and mapping | B / 0 | RO01 | Physical perception foundations; calibration, environment and hardware limits |
| RO04 | Learned perception and scene models | E / 8 | RO03 + ML03 + DA02 | Better task-specific perception; held-out sites and weather/lighting coverage |
| RO05 | Learned robotic task policies | E / 10 | RO04 + ML07 + RO02 | New industrial/assistive task configurations; external hardware and measured reliability |
| RO06 | Teleoperation and supervised fallback | S / 12 | RO04 + HW04 + ET07 | Human assistance for difficult cases; staffing, latency and communications costs |
| RO07 | Robotic service contracting | S / 17 | RO05 + RO06 + BU02 | Paid task outcomes through actual partner fleets; insurance/support/maintenance |
| RO08 | Verified industrial integration | S / 23 | RO05 + MA04 + ET08 | Broader automation partnerships; commissioning and local safety evidence |
| RO09 | Predictive physical maintenance | S / 27 | RO08 + ML02 + HW04 | Reduce some outages through measured maintenance; replacement parts and labor |
| RO10 | Adaptable bounded manufacturing | S? / 44 | RO08 + ML10 + DA11 | Flexible production within a tested envelope; no uncontrolled self-replication |
| RO11 | Circular fleet/component recovery | S / 58 | RO09 + MT10 | Recover some useful hardware/materials; finite processing, losses and throughput |
| RO12 | Century fleet replacement networks | S / 95 | RO11 + BU12 + SO12 | Sustain delivered physical services over time; no immortal maintenance-free fleet |

## Repeatable end-of-branch families

These are ten examples of refinement templates. Attach a concrete instance to
an eligible branch/variant, not automatically to every node. They use the bounded
curves, actual paid effort and precision limits in TECHNOLOGY.md. Reaching the
campaign deadline is not required to use a refinement if its branch is ready.

| ID | Required example gate | Refined quantity | Practical floor/cap and remaining cost |
| --- | --- | --- | --- |
| RF01 | ML09 | Task-specific model efficiency | Recipe-specific held-out performance ceiling; data and serving stay finite |
| RF02 | DA10 | Useful non-duplicate data yield | Eligible source/coverage limit; rights and verification remain |
| RF03 | MA12 | Verified-work throughput | Task/proof-scope ceiling; independent checking cannot be removed |
| RF04 | HW08 | Realized utilization | Physical capacity and protected commitments; never allocate over 100% |
| RF05 | MT10 | Recoverable component yield | Material loss/quality floor; processing, delivery and inventory remain |
| RF06 | BI06 | Domain prediction error | Validated domain/error floor; no guaranteed clinical or scientific outcome |
| RF07 | SO10 | Adoption/transition implementation efficiency | Eligible population and local resources; not unlimited approval or welfare |
| RF08 | ET10 | Specific control failure rate | Residual exposure/coverage floor; cannot suppress unrelated hazards |
| RF09 | BU08 | Settlement/support overhead | Actual staffing/legal/counterparty floors; cannot create demand or immunity |
| RF10 | RO09 | Maintenance-adjusted useful uptime | Physical/inspection limits and replacement cost; no perfect safety |

## Initial production subset and coverage gates

For the opening research/licensing implementation, select a small connected
subset: ML01–ML06, DA01–DA06, MA01–MA09, HW01–HW05, SO01–SO05,
ET01–ET07 and BU01–BU07, trimming optional choices to a complete player route.
Many of these are already-known baseline practices, not dozens of new research
projects to perform before the first product. Provide a recurrent specialist
path, a later Transformer path, publication/adoption and one genuine license
deal. Physical/domain branches initially use purchased operating capability.

Only expand a candidate into content when its staff/tools, implementation cost,
rights, outputs, evaluation, failure/recovery and rivals' use can be expressed
and explained. Preserve alternatives, avoid circular references and check the
complete knowledge graph plus scenario-disabled branches. Each model setting,
channel, hardware family and starting permission needs a real owning definition;
an entry in this document alone unlocks nothing in the running game.
