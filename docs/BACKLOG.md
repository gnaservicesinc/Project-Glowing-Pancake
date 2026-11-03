# Implementation backlog

Baseline 1.0 · All items below are **not started**. Role names refer to [agent briefs](../agents/README.md), not automatically running agents. Dependencies refer to completed acceptance criteria, not merely the presence of a commit. Milestone gates in [the roadmap](ROADMAP.md) apply in addition to the task dependencies below. Update this file and [status](STATUS.md) when work actually advances.

## M0 — Toolchain and platform proof

- [ ] **M0-01 — Record toolchain and platform feasibility.** Role: build-release. Depends: planning. Inspect installed tools, actual Qt modules/licenses, runner availability, ARM SDK acquisition, disk budget, and native test access. Produce a reproducible inventory without installing large packages locally. Separate confirmed facts from missing tests.
- [ ] **M0-02 — Create Release-default build entry points.** Role: build-release. Depends: M0-01. Add CMake/Ninja configuration, `make.sh` and documented flags, a minimal core/headless executable, and ignored out-of-source outputs. No-argument build is Release, core-only does not require Qt, clean removes only owned outputs, tests propagate failure, and no build auto-launches.
- [ ] **M0-03 — Prove QML, graphics, and local audio deployment.** Role: UI with build-release brief. Depends: M0-02. Create a small original UI probe; use Metal on Mac, OpenGL on Linux, and test OpenGL/D3D11 recovery on Windows. Load local PNG/JPEG, an accessible button, and optional WAV/Ogg. Package dependency inventory and record actual renderer/backend evidence.
- [ ] **M0-04 — Establish five native CI package probes.** Role: build-release. Depends: M0-03. Build arm64 Mac, amd64/arm64 Linux, amd64/arm64 Windows artifacts from pinned inputs. Inspect runtime paths and licenses. Test without the developer Qt installation; mark physical graphics/audio gaps rather than equating compile and support.

## M1 — Foundation

- [ ] **M1-01 — Deterministic state primitives.** Role: simulation. Depends: M0-02. Implement typed IDs, checked money/units, Gregorian day stepping, named PRNG streams and canonical iteration. Test leap years, rounding residues, overflow refusal, and identical seed/command hashes across compiler/architecture jobs.
- [ ] **M1-02 — Content registry and validation.** Role: simulation/content. Depends: M1-01. Define versioned schemas and typed loaders for initial content. Reject duplicate/dangling IDs, unit/range errors, graph cycles, invalid ownership totals, and executable/remote content. Examples become fixtures only after validation exists.
- [ ] **M1-03 — Commands, transactions, and snapshots.** Role: simulation. Depends: M1-01, M0-03. Commands revalidate against state revision, reserve atomically, and return stable rejection reasons. UI receives immutable snapshots from a worker. Stale commands cannot overspend or partially apply.
- [ ] **M1-04 — Local persistence and settings.** Role: simulation with UI brief. Depends: M1-02, M1-03. Add versioned saves, atomic replacement, recoverable prior save, three-slot autosave ring, preferences, and export. Reload preserves RNG, obligations, and next-step hashes; corrupt/oversized/future saves fail without losing current state. No network path.
- [ ] **M1-05 — Accessible room shell.** Role: UI. Depends: M1-03. Office/Lab navigation, shared status/time controls, inbox, inspector, compact management view, keyboard focus, text scaling, pause/step controls, and local settings. Verify at 1280×720 and 200% scaling without blocking core actions.

## M2 — Headless business loop

- [ ] **M2-01 — Accounts, obligations, and financing primitives.** Role: simulation. Depends: M1-02, M1-03. Balanced postings, reservations, month settlements, receivables/payables, debt, equity conservation, and cash projections. Income, credit, equity, and cash are distinct; costs cannot be silently duplicated.
- [ ] **M2-02 — Shared capacity and project commitments.** Role: simulation. Depends: M2-01. Lease allocation, staff assignments, project reservations, actual work, and serving protection. Daily assigned CU never exceed effective CU; lease power is not billed twice; cancellation releases only future commitments.
- [ ] **M2-03 — Contracts and demand allocation.** Role: simulation. Depends: M2-02. One enterprise segment, outside option, eligibility, price, useful-work demand, finite supply, refunds, and contract deadlines. Common-snapshot allocation conserves tasks/money and does not favor entity ordering.
- [ ] **M2-04 — Three-year scenario and liquidity recovery.** Role: QA/simulation. Depends: M2-03, M1-04. Exercise the opening balance, settlements, scenario runner, 30-day workout, rescue and closure paths. Reports reconcile; saved continuation matches uninterrupted play; a solvent strategy is demonstrable.

## M3 — First playable

- [ ] **M3-01 — Data/research/train/evaluate loop.** Role: simulation. Depends: M2-04. Four data offers, six research nodes, model lineage, finite training and held-out evaluation. Expose uncertainty and bottlenecks; checkpoint cloning cannot add free capability; estimates do not alter RNG.
- [ ] **M3-02 — Playable scenario presentation and tutorial.** Role: UI/content. Depends: M3-01, M1-05. Office/Lab panels, three headquarters choices, training preview, contract comparison, eight triggered events, two bounded rival routines and a heavyweight warning. Start to ending is playable without developer intervention.
- [ ] **M3-03 — Delegation, warnings, recovery, and slice scoring.** Role: UI/simulation. Depends: M3-02. Add budget-bounded renewal/evaluation policies, configurable auto-pause, a recoverable setback, and specialist/financing paths. Fast-forward cannot skip a required decision or deadline.
- [ ] **M3-04 — Validate the slice with people.** Role: QA. Depends: M3-03. At least five independent 30–45-minute sessions; three complete without facilitator help; record consented, manually summarized feedback without game analytics. Players can explain cash burn, model evaluation, and why their contract succeeded/failed. Fix material comprehension blockers before M4.

## M4 — Competitors and governance

- [ ] **M4-01 — Full rival planning and staged entry.** Role: simulation. Depends: M3-04. Schedule the default roster and implement budgeted strategies through shared commands. Test imperfect information, insolvency, research prerequisites, bounded catch-up settings, and publication diffusion.
- [ ] **M4-02 — Heavyweight mobilization and deals.** Role: simulation/content. Depends: M4-01. Watch/mobilize/compete/consolidate, visible 90-day warning, finite spending/lead times, offers, hysteresis and exit. A player can anticipate and answer a response; success does not cause instant omnipotent retaliation.
- [ ] **M4-03 — Funding, executives, negotiation, and board.** Role: simulation/UI. Depends: M4-01, M2-01. Equity rounds, debt/covenants, strategic deals, board seats, key roles, promises and straightforward mergers. Ownership and obligations conserve through deals; disputes have causes and response windows.

## M5 — Geography and global markets

- [ ] **M5-01 — Offline domestic map and owned infrastructure.** Role: UI/simulation/content. Depends: M4-03. Prepared 50-state/DC geometry with list alternative; licensed source evidence; power, cooling, grid queues, hardware delivery, staff, maintenance and optional executive travel/appointments. State/site choice measurably changes a feasible plan; travel never disables basic management.
- [ ] **M5-02 — International profiles and product channels.** Role: simulation/UI. Depends: M5-01, M4-01. Eight regions, all six channels, twelve useful product templates, local eligibility, open/on-premise attribution, and regional useful-work accounting. No double-counted share or unlimited free serving.
- [ ] **M5-03 — Hazards, resilience, and insurance.** Role: simulation/content. Depends: M5-01. Seasonally appropriate exposure, backup/replication, outage consequences and financial insurance. Mitigations affect their stated mechanisms; insurance does not magically restore hardware availability.

## M6 — Politics and society

- [ ] **M6-01 — US institutions, policy, and election calendar.** Role: simulation/content. Depends: M5-02, M4-03. Seat blocs, proposals, votes, executive/agency implementation and state policies. Show effective dates, eligibility, evidence and influence costs. Elections and delayed rules survive save/load.
- [ ] **M6-02 — Macro response, global coordination, and media.** Role: simulation/content. Depends: M6-01. Adoption-to-world-effects lags, five welfare indices, standards forum, access restrictions, advertising and audience reputation. Company spend affects delivered capacity/outcomes before welfare changes; no single world-law toggle.
- [ ] **M6-03 — Executive/public conflicts and authority paths.** Role: simulation/UI/content. Depends: M6-02. Multi-stage disputes, testimony, commitments, audience-specific identity, institutional authority, abstract coercive control and resistance. Advertising/market share alone cannot satisfy domination.

## M7 — Century and endings

- [ ] **M7-01 — Conditional future and game AGI/ASI.** Role: simulation/content. Depends: M6-02, M5-02. Scenario progress branches, advanced recipes/hardware, bounded research automation, independent assessment windows and plateau behavior. No fixed future arrival date or infinite feedback loop; ASI objective scenarios have a feasible route.
- [ ] **M7-02 — Succession and founder legacy.** Role: simulation/content. Depends: M4-03, M7-01. Plan a successor, change executive/chair relationships, preserve company obligations and founder trust. Century play does not assume one immortal executive or create duplicate wealth.
- [ ] **M7-03 — Six objectives and postmortems.** Role: simulation/QA/UI. Depends: M7-01, M7-02, M6-03. Implement sustained windows, near misses, ties, threshold boundaries, objective changes, closure/catastrophe explanations, and sandbox continuation. Each objective has a reachable winning fixture and a losing near-miss fixture.

## M8 — Content and endurance

- [ ] **M8-01 — Complete useful content and asset coverage.** Role: content/UI. Depends: M7-03. Expand toward scope budgets, fill all six strategic paths, replace unclear placeholders, integrate selected licensed music, and finish offline credits. No unverified asset or empty moral-choice filler; audit keyboard/text/motion alternatives.
- [ ] **M8-02 — Balance and long-run validation.** Role: QA/simulation. Depends: M8-01. Matched-seed specialist/open/frontier policy runs, exploit cases, 100-year batches, a human full campaign, and 500-year sandbox checks. No unreconciled ledgers, growing queues or opaque unrecoverable failures; document balance changes and playtime.
- [ ] **M8-03 — Performance, privacy, and dependency audit.** Role: QA/build-release. Depends: M8-02, M0-04. Measure named-device response/memory/save/fast-forward costs; inspect release logging, resource URLs, sockets, fonts, codecs, licenses and credits. Play with denied network and observe connection attempts; no runtime collection/transmission/updater.

## M9 — Primary release

- [ ] **M9-01 — Qualify all five clean-device artifacts.** Role: build-release/QA. Depends: M8-03. Download candidates onto clean native systems, test graphics/audio/UI/save paths and source/license bundles, verify architecture and no developer paths. Publish only the platforms actually qualified; full 1.0 gate requires all five.
- [ ] **M9-02 — Prepare a reviewable itch release.** Role: build-release/content. Depends: M9-01. Version/tag plan, immutable artifacts and checksums, matching source, screenshots, title/description, privacy statement, supported OS/arch, install and accessibility notes. Keep draft reviewable; no automatic public upload is implied.
- [ ] **M9-03 — Publish the approved primary release.** Role: build-release. Depends: M9-02 and explicit release task. Use authorized account/credentials, upload intended channels once, verify downloaded artifacts, record URLs/version, and clean generated staging files. Never fabricate publication success.

## M10 — Secondary distribution

- [ ] **M10-01 — Package and qualify Snap.** Role: build-release/QA. Depends: M9-03 and an explicit Snap task. Use Ubuntu builds for amd64/arm64, least necessary interfaces, local save/settings behavior, graphics/audio and refresh checks, accurate store metadata, source/license compliance, and authorized submission. No micro-HTTP server or new game update service.

## Task completion record

For each completed item, record commit, build/test or playtest evidence, remaining limits, and next item in the task handoff. A checked box means the acceptance criteria passed. Starting a task, creating stubs, or obtaining a passing compiler exit alone does not close it. Do not create external issue spam or contact testers unless the user asks.
