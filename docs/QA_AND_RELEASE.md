# Verification, playtesting, and release gates

Baseline 1.0 · These are planned checks, not results from a built game

## 1. Evidence rules

Close a task when its intended behavior has evidence. Distinguish documentation review, unit tests, headless integration, rendered UI inspection, physical-device checks, human playtesting, and actual release download verification. Passing one does not imply the others. Record the commit, content version, scenario seed, compiler/OS/architecture, relevant hardware, and observed result.

Current planning verification is limited to document consistency, links, valid example JSON, source preservation, and simple arithmetic. No game binary, application tests, benchmarks, accessibility certification, or five-platform runtime acceptance exists yet.

## 2. Core invariants and meaningful tests

| Area | Required evidence |
| --- | --- |
| Calendar | Leap years, exact 100-year boundary, due-date phases, month settlements, succession and long sandbox dates |
| Determinism | Matching state hashes after a fixed command sequence across toolchains/architectures; forecasts/rendering leave RNG unchanged |
| Money and equity | Balanced postings; conserved ownership; commitments versus expenditures; no double-counted cloud power, credit revenue, dividends or terminal wealth |
| Capacity | No compute/staff overbooking, finite serving, maintenance/outages, cancellation/resume, protected capacity and delivery delays |
| Demand | Common-snapshot fairness; finite budgets/tasks; outside option; eligibility; refunds; free/open distribution does not invent use or revenue |
| Research/models | Prerequisites, checked units, diminishing returns, independent evidence, contamination, lineage and checkpoint duplication prevention |
| People/governance | Workload/morale lags, escalating disputes, actual voting rights, conserved merger obligations, succession continuity |
| Rival rules | Shared command validation, finite financing/capacity, imperfect information, warning period and hysteresis |
| Politics/world | Calendar and delayed enforcement, jurisdiction, reversible mandates, macro lags, no direct welfare-from-spend or domination-from-advertising shortcut |
| Objectives | Every threshold, exact boundary, sustained window, interrupted window, regional floor, same-day ties, scenario compatibility, and correct postmortem |
| Persistence | Atomic save failure, prior-save recovery, unknown formats, corruption, file bounds, migrations, queued events, commitments, objective windows, and next-day hash parity |

Tests should target behavior and failure modes, not mirror implementation. Use tiny hand-auditable fixtures for ledger/capacity math and seeded integration runs for coupled systems. No arbitrary test-count target. If a bug escapes, add a focused regression only when it captures a meaningful failure mechanism.

## 3. Scenario suite

Maintain comparable seeds and content versions for:

- Careful specialist: modest compute, stronger fit/reliability, expanding contracts, no frontier-race requirement.
- Open ecosystem: published methods/weights, limited direct capture, partnership and talent benefits, real serving costs.
- Capital-intensive frontier lab: financing, infrastructure queues, research uncertainty, board commitments and deployment choices.
- Deliberate stress/exploit policies: zero-price share capture, cash shell survival, self-dealing, endless financing, perfect hindsight through reload/preview, cloned checkpoints, runaway research automation, influence spam and tiny-region victory gaming.
- Recovery cases: customer loss, interrupted training, key departure, supply delay, debt covenant, power outage, leadership change, and an unfavorable election.
- Each of six objective families: a winning fixture, near miss, threshold/window boundary, regional exclusion where applicable, and save/reload around resolution.

Automated policy runs prove specified mechanics, not fun or factual truth. Keep a small fixed regression set on ordinary commits and larger seed sweeps at relevant milestones. Do not run expensive century batches for a reversible text-only edit.

## 4. Human playtests

At M3, recruit at least five independent sessions, including two players without ML background. Target three unaided completions of the 30–45-minute scenario. Record completion, points of confusion, whether players can explain cash burn and model evaluation, decisions they found meaningful, and whether they saw a recoverable setback. Feedback is manual and consented; no game telemetry.

At M8, complete at least one full human century campaign and focused late-game sessions across the other objectives. Check that delegation handles routine work and that endgame choices differ from opening choices. Ask players to explain a consequence before showing a designer explanation; confusion exposes a UI/model problem. Evaluate plausible alternative strategies, rather than whether players agree with the game's implied politics.

The user or an explicitly authorized task arranges tester contact. Do not message people or upload saves to third-party services on the assumption that a playtest plan authorizes it. Summaries should exclude personal identifying details and use local synthetic/fictional saves for public bug fixtures.

## 5. UI and accessibility

Inspect actual application screens at 1280×720 and higher resolutions, 100/150/200% scale, high contrast, reduced motion, and keyboard-only operation. Room hotspots, maps, sliders, tables, dialogs, and charts need text/keyboard alternatives. Confirm focus order, visible focus, number/date units, truncation, scroll behavior, modal dismissal, and that pause remains reachable.

Verify screen-reader labels and navigation on macOS VoiceOver and representative Windows/Linux readers before claiming corresponding support. Do not promise universal screen-reader compatibility from attached properties alone. If a map/chart cannot be read directly, its table must provide equivalent decision information. Audio never carries a critical fact alone. Save/load and error dialogs must work without a mouse.

## 6. Privacy and offline acceptance

The privacy requirement is no collection/transmission of player information, not a ban on local persistence. Local settings, saves, autosaves, and campaign history are expected. The shipped game has no server, account system, analytics, tracking IDs, cloud dependency, crash uploader, or updater.

Required release evidence:

1. Review runtime dependencies and code/resource paths for analytics SDKs, remote URLs, sockets/listeners, web content, remote fonts, update checks, reporting and uploaded diagnostics.
2. Run the packaged Release build in a network-denied environment from first launch through new game, tutorial, saved continuation, settings, credits, and an ending. Observe attempted connections separately; successful offline play alone does not prove that no connection was attempted.
3. Confirm there is no automatic account/login flow, device identifier, OS-account-name harvesting, remote configuration, or hidden analytics file. Local saves/preferences contain expected gameplay/settings data only.
4. Confirm default Release excludes developer logging/export controls; debug builds also have no telemetry path. Any support diagnostic export is explicit, local, and inspectable.
5. Confirm only a deliberate store/source-link click opens the system browser, with no player/session payload. Offline credits remain complete.
6. Separate the game's process from itch/Snap client update traffic in the test report. The distribution clients' connections do not justify networking in game code.

An external website URL used for attribution is not itself a background connection. Do not delete useful source credits to satisfy a superficial URL search; inspect whether anything actually loads or sends data.

## 7. Campaign and performance acceptance

Run seeded 100-year campaigns with the standard roster and bounds from the simulation specification; run 500-year sandbox stress scenarios. Check checked-number bounds, queue sizes, historical compaction, data references after mergers/succession, objective counters, ledger reconciliation and deadlocks. Track peak memory and save size over time to detect continuing growth.

Measure response, UI frame pacing, saves, loads and headless throughput on named reference hardware. Initial targets are <100 ms ordinary-action response, 60 fps main UI with a 30 fps low-power recovery target, <1 GB standard-campaign memory, <25 MB normal save, <2-second save/load on SSD, and <60-second headless century on a modern developer CPU. Set published minimum requirements only after measurement. Report unmet targets and their effect; do not turn unmeasured targets into marketing claims.

## 8. Clean-package acceptance

For each of the five OS/architecture combinations, download the actual candidate onto a clean native environment with no development Qt paths. Verify executable architecture, renderer and fallback, audio decoding, QML/image/platform/accessibility plugins, DPI behavior, writable save/settings paths, relocation, first launch, save reload, and uninstall leaving user saves available. Linux qualifications state the distro/glibc/display/driver baseline.

Inspect bundled libraries, runtime search paths, transitive dependencies and licenses. Packages include corresponding game source/build information, third-party source/notice material as required, offline credits, installation instructions and checksums. No private paths, API keys, access tokens, local developer saves, large test outputs, or unreviewed asset files may enter a release.

Test a downloaded Mac bundle's signing/quarantine behavior; the baseline is ad-hoc signed and not notarized. Installation guidance must describe that accurately and must not recommend global security disabling. Do not automatically launch from installation scripts. ARM emulation or CI compilation does not substitute for native graphics/audio acceptance.

## 9. Release checklist

- [ ] Milestone M8 gates passed; blocking issues resolved or explicitly scoped with accurate claims.
- [ ] Five primary artifacts qualified for the full 1.0 release; no unsupported platform badges.
- [ ] Privacy/offline evidence recorded for the Release candidate.
- [ ] Save compatibility, migrations and backup/recovery checked.
- [ ] Content review, per-file provenance, music credits, licenses and source bundle complete.
- [ ] Performance/accessibility claims reflect observations.
- [ ] Working title/final title, cover, screenshots, description, age/content framing and install notes reviewed.
- [ ] Version, source commit, content hashes, artifacts and checksums agree.
- [ ] Concrete public upload/submission task explicitly authorized; credentials stored through supported secret handling.
- [ ] Uploaded build downloaded again and checked; exact release URLs/version recorded.
- [ ] Generated build/staging files cleaned; task documentation committed and pushed.

Snap has its own later Ubuntu-based qualification: amd64/arm64, confinement interfaces, graphics/audio, local paths and refresh behavior. It introduces no game server or custom updater. A successful itch release does not automatically prove a Snap package.
