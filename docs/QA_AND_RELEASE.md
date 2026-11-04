# Verification, playtesting, and release gates

Baseline 1.2 · These are planned checks, not results from a built game/editor

## 1. Evidence rules

Close a task when its intended behavior has evidence. Distinguish documentation review, unit tests, headless integration, rendered UI inspection, physical-device checks, human playtesting, and actual release download verification. Passing one does not imply the others. Record the commit, content version, scenario seed, compiler/OS/architecture, relevant hardware, and observed result.

During Mac implementation, use one incremental optimized local build per batch and small focused checks for the changed behavior and relevant failure path. Retain owned build trees between batches. Use Debug or broader regression when a specific failure or unresolved concern calls for it; duplicate Release/Debug runs and remote-matrix waits are not routine feature work. Documentation-only changes need consistency checks.

The full verification inventory below remains for owning milestone and release preparation. Cross-platform matrices, full regression, packaging and device qualification are deferred until explicit release preparation and do not block bounded Mac implementation. The six-job native workflow is manual-only. Keep portable code, offline operation, safe saves and explicit app launch during development; record unperformed gates without claiming platform qualification. Existing foundation evidence is tracked in the implementation log.

## 2. Core invariants and meaningful tests

| Area | Required evidence |
| --- | --- |
| Calendar | Leap years, exact 100-year boundary, due-date phases, month settlements, succession and long sandbox dates |
| Determinism | Exact canonical hashes for a 1,000-day scripted fixture on Clang/GCC/MSVC and five target architectures; separately sequenced RNG draws, total-order ties, integer-only state; previews leave RNG unchanged |
| Money and equity | Constant-2014 cents throughout; balanced postings; wide products with fitting quotients, invalid denominators/final overflow; multi-factor rounding; reserved-pool grants/vesting/departure/dilution; no duplicated cloud power, credit revenue or terminal wealth |
| Capacity | Compute-per-task consumption and support-cost separation; no compute/staff overbooking; wholesale buyer/seller conservation, no circular resale; maintenance/outages, cancellation, serving protection and delivery delays |
| Demand | Common-snapshot fairness; unequal prices, rational budget scaling, zero-price work, constrained residues and cross-segment supply; installed-share churn and forced exits; termination bound; all refused tasks accounted outside, no invented use/revenue |
| Research/models | Prerequisites, checked units, diminishing returns, independent evidence, contamination, lineage and checkpoint duplication prevention |
| People/governance | Workload/morale lags, escalating disputes, actual voting rights, conserved merger obligations, succession continuity |
| Rival rules | Shared command validation, finite financing/capacity, imperfect information, warning period and hysteresis |
| Politics/world | Calendar and delayed enforcement, jurisdiction, reversible mandates, macro lags, no direct welfare-from-spend or domination-from-advertising shortcut |
| Objectives | Exact final 60/120 month-end checks, aggregate Monopoly versus every-check Domination/Utopia; immediate irreversible-breach explanation/continuation, alert reload, thresholds, regional floors, ties and correct postmortem |
| Persistence | Atomic save failure, prior-save recovery, unknown formats, corruption/bounds, migrations, morning phase cursor, nested continuation frames, installed shares, objective windows/alerts and next-day hash parity |
| Content flows | Explicit typed targets/relative anchors; nested numbered fan-out/delay/dialogue return; depth refusal; five lifecycles; natural enter/hold/exit versus labeled synthetic cleanup; no duplicate effects/rerolls or modifier drift |
| Versions | Distinct game/editor/content identities, definition revisions, content hashes, format/rules compatibility, package/tag/source consistency and pending-chain migration |

Tests should target behavior and failure modes, not mirror implementation. Use tiny hand-auditable fixtures for ledger/capacity math and seeded integration runs for coupled systems. No arbitrary test-count target. If a bug escapes, add a focused regression only when it captures a meaningful failure mechanism.

Boundary arithmetic and allocation expectations are illustrated in [simulation boundary examples](examples/simulation-boundaries.example.json). They remain design fixtures until real implementations execute them. Test that a due delivery/law/flow is visible while paused on its morning, and that reload/command draining does not apply it again. Undo may release unconsumed reservations; same-date cancellation must not refund delivered information/rights or rewind a random result.

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

While snapshots arrive at the normal 10 Hz cap, exercise table edits, sorting, insert/remove/move, keyboard selection and scroll anchors. Persistent bridge models must update changed roles without routine resets or lost focus. Test required/actionable/informational notification routing and fast-forward deadlines; routine news must not cause popup fatigue. Held-out evidence should explain a concrete subgroup limitation without exposing all latent flaws.

## 6. Privacy and offline acceptance

The privacy requirement is no collection/transmission of player information, not a ban on local persistence. Local settings, saves, autosaves, and campaign history are expected. The shipped game has no server, account system, analytics, tracking IDs, cloud dependency, crash uploader, or updater.

Required release evidence:

1. Review runtime dependencies and code/resource paths for analytics SDKs, remote URLs, sockets/listeners, web content, remote fonts, update checks, reporting and uploaded diagnostics.
2. Run the packaged Release build in a network-denied environment from first launch through new game, tutorial, saved continuation, settings, credits, and an ending. Observe attempted connections separately; successful offline play alone does not prove that no connection was attempted.
3. Confirm there is no automatic account/login flow, device identifier, OS-account-name harvesting, remote configuration, or hidden analytics file. Local saves/preferences contain expected gameplay/settings data only.
4. Confirm default Release excludes developer logging/export controls; debug builds also have no telemetry path. Any support diagnostic export is explicit, local, and inspectable.
5. Confirm only a deliberate store/source-link click opens the system browser, with no player/session payload. Offline credits remain complete.
6. Separate the game's process from itch/Snap client update traffic in the test report. The distribution clients' connections do not justify networking in game code.

M0 resource-guard tests precede the release audit: reject remote QML/images/fonts/requests, redirect attempts, remote file hosts and canonical-path escapes before transport, while approved bundled/local resources work. QML guards are not a process-wide network firewall; observe native/dependency behavior too. Audio may not request microphone/capture access.

An external website URL used for attribution is not itself a background connection. Do not delete useful source credits to satisfy a superficial URL search; inspect whether anything actually loads or sends data.

## 7. Campaign and performance acceptance

Run seeded 100-year campaigns with the standard roster and bounds from the simulation specification; run 500-year sandbox stress scenarios. Check checked-number bounds, queue sizes, historical compaction, data references after mergers/succession, objective counters, ledger reconciliation and deadlocks. Track peak memory and save size over time to detect continuing growth.

Measure response, UI frame pacing, saves, loads and headless throughput on named reference hardware. Initial targets are <100 ms ordinary-action response, 60 fps main UI with a 30 fps low-power recovery target, <1 GB standard-campaign memory, <25 MB normal save, <2-second save/load on SSD, and <60-second headless century on a modern developer CPU. Set published minimum requirements only after measurement. Report unmet targets and their effect; do not turn unmeasured targets into marketing claims.

## 8. Clean-package acceptance

For each of the five OS/architecture combinations, download the actual candidate onto a clean native environment with no development Qt paths. Verify executable architecture, renderer and fallback, audio decoding, QML/image/platform/accessibility plugins, DPI behavior, writable save/settings paths, relocation, first launch, save reload, and uninstall leaving user saves available. Linux qualifications state the distro/glibc/display/driver baseline.

Inspect bundled libraries, runtime search paths, transitive dependencies and licenses. Packages include corresponding game source/build information, third-party source/notice material as required, offline credits, installation instructions and checksums. No private paths, API keys, access tokens, local developer saves, large test outputs, or unreviewed asset files may enter a release.

Test a downloaded Mac bundle's signing/quarantine behavior; the baseline is ad-hoc signed and not notarized. Verify Apple's attempted-launch → Privacy & Security → Open Anyway per-app route where available, and document managed-system restrictions honestly. Test browser ZIP and any supported itch-client route; neither Control-click nor client extraction is a guaranteed bypass. Do not recommend global security disabling or auto-launch from installation scripts. ARM emulation or CI compilation does not substitute for native graphics/audio acceptance. [Apple guidance](https://support.apple.com/en-us/102445).

## 9. Release checklist

- [ ] Milestone M8 gates passed; blocking issues resolved or explicitly scoped with accurate claims.
- [ ] Five primary artifacts qualified for the full 1.0 release; no unsupported platform badges.
- [ ] Privacy/offline evidence recorded for the Release candidate.
- [ ] Save compatibility, migrations and backup/recovery checked.
- [ ] Content review, per-file provenance, music credits, licenses and source bundle complete.
- [ ] Performance/accessibility claims reflect observations.
- [ ] Working title/final title, cover, screenshots, description, age/content framing and install notes reviewed.
- [ ] Version, source commit, content hashes, artifacts and checksums agree.
- [ ] Shared version manifest has no planning placeholders/unresolved content bump; game tag does not identify an editor release. Editor authoring resources/drafts/layouts are excluded from the game.
- [ ] Concrete public upload/submission task explicitly authorized; credentials stored through supported secret handling.
- [ ] Uploaded build downloaded again and checked; exact release URLs/version recorded.
- [ ] Generated build/staging files cleaned; task documentation committed and pushed.

Snap has its own later Ubuntu-based qualification: amd64/arm64, confinement interfaces, graphics/audio, local paths and refresh behavior. It introduces no game server or custom updater. A successful itch release does not automatically prove a Snap package.

## 10. Content Studio and versioning acceptance

Studio tests use the same content compiler/runtime as the game, copied fixture state and copied RNG streams. Check graph/form/outline round-trip parity, current-content browsing/search, stable IDs, uses/used-by, typed ports, units and target bindings. Verify the exact event linked, not only that some similarly named event appears.

For trigger inspection, verify nested all/any/not, true/false labels, inclusive boundaries, temporal/relative-anchor checks, missing targets and matched-scope counts. For effect inspection, verify before/after signs, percentage versus percentage-point arithmetic, explicit targets, order, ledger postings and resource refusal. Continuous fixtures must enter/hold/exit through normal conditions; separately labeled forced-removal probes test only cleanup. Preserve unrelated modifier/base changes. Plant a valid-but-inverted multiplier and a valid-but-wrong-company target; intent assertions and inspection must reveal them even when types pass. Unknown facts are errors, not false matches. Previews do not prove exhaustive long-run impacts.

Test each lifecycle and branching conversation before/after save/reload, including delayed hidden consequences, cooldowns, active modifiers and pending results. Reopening cannot reroll or repeat committed effects. Fast-forward cannot skip a decision or apply an end-of-day event retroactively. Test runtime step/queue budget refusal and recovery without a CPU-dependent timeout or silent dropped event.

Save while the first child of a nested sequence waits on a delay or dialogue, then complete it and verify each later sibling/return executes exactly once in order. Test the eight-frame boundary and excessive depth refusal. Opening/panning/zooming a graph must leave tracked layouts and runtime hashes untouched; intentional integer-coordinate moves may change layout only.

Repository write tests use disposable checkouts: external concurrent edits, unknown/new schema, unsupported fields, missing/renamed references, path escapes, asset provenance, undo/redo, crash interruption and journal recovery. No test may overwrite the real project or launch/publish another product. Drafts can remain invalid without entering runtime packs; approved content cannot release with errors.

Version cases: no-op/layout save; text-only patch; batch changing three definitions once each; new additive event; schema/rules break; content-only game package update; editor-only UI update; stale unfinalized content; reused version with changed hash; game/editor tag mix-up; preserved pending-dialogue effects after content migration. Confirm displayed About/build/filename/source/content versions agree. Snap's later metadata uses game version while store revision/channel remain separate identities.

E1a requires an engineering walkthrough of complete forms/outline, inspector and safe reviewed internal snapshots before M3 content; it is not a claim of human usability. E1b's human gate uses three authors, including a scenario writer and a non-scripter. Each browses content, builds trigger→multiple effects/events with the canvas, creates a dialogue/result branch and delay, catches planted sign/scope errors, then tests/saves/reopens/finalizes without commands or text edits. E1b and M3 both close before M4. Summarize manual consented feedback; no analytics. E2 independently qualifies editor artifacts and current-runtime parity before M8.
