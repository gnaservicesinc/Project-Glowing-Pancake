# Needs, access, and acquisition list

Checked 2026-11-03. This is a staged list of resources, not a request to purchase or configure everything now. No blocking user answer is needed to begin M0. No accounts were created, credentials changed, assets purchased, or releases published during planning.

## Available and checked

| Resource | Evidence / current limit |
| --- | --- |
| Local repository | `/opt/AI Startup/Project-Glowing-Pancake`; initial `main` matched `origin/main`; existing GPLv3 license |
| Remote repository | `gnaservicesinc/Project-Glowing-Pancake`, public; connector reports push/admin permission; SSH remote read succeeded. Actual task push must still be checked when performed. |
| macOS Qt | `/opt/Qt/6.12.0/macos`; qmake reports 6.12.0. Review-time inspection found no Qt6Multimedia CMake module and found installed SPDX/CycloneDX SBOMs. Other OS SDK acquisition and actual deployed coverage remain M0 work. |
| Compiler spot check | Default LLVM Clang 23.1.3 at `/usr/local/bin/clang++`; Apple Clang 21.0.0 via `/usr/bin/clang++`. `xcrun` reported compiler/SDK paths under different Xcode app roots; M0 must select/verify one coherent toolchain before Qt configuration. No compilation/linkage proof yet. |
| CMake and Ninja | Installed; versions reported 4.4.4 and 1.13.2 respectively. This does not prove the compiler/deployment toolchain. |
| Other authoring tools | GameMaker and Ren'Py application bundles exist. Not selected; no extra engine setup is requested. |
| Original design sources | Both drafts archived verbatim in Git with SHA-256 fingerprints |
| Editor references | Official Plague Inc./Paradox guides reviewed; supplied `/opt/URLAlchemist/Repo/URLAlchemist` source inspected read-only for node/catalog/layout concepts. No code copied or reference-project commands executed. |
| Version tracking seed | `project-versions.json` initialized with planning-only component versions and reserved first compatibility formats; no version/build automation yet |
| Local disk | Approximately 32.5 GiB free at the baseline 1.2 spot check, superseding the earlier 12 GB observation. Transient measurement, not a durable budget; avoid heavy local SDK/package staging. |

## Before and during M0

| Need | Why / how to obtain | Responsible role | Blocks |
| --- | --- | --- | --- |
| Confirm usable local C++20 compiler and Xcode command-line tools | Resolve compiler/SDK through one selected existing Xcode directory, inspect actual linkage, and record paths/versions; do not change global settings or install another SDK for convenience | Build-release | Local core/Qt check |
| Confirm GitHub Actions is enabled and workflows can run | Repository access is present; Actions permissions/runs were not exercised in planning | Build-release | Native CI probes |
| Exact Qt patch/modules and public redistribution inventory | Query five-target archives/modules/ABI before CI, pin inputs/checksums and reconcile installed SBOMs against deployed modules/license alternatives. Keep 6.12.x unless an evidenced decision changes it; no automatic 6.4 fallback. Prefer hosted-runner acquisition; no commercial subscription assumed | Build-release | Reproducible native packages |
| Native Windows ARM64 and Linux ARM64 toolchain path | Verify hosted labels, MSVC/Qt packages, Linux libraries, and runner storage. Current public-runner documentation lists ARM options; actual jobs remain to be tried | Build-release | ARM artifact proof |
| Minimal local image/audio fixtures and optional playback adapter | Original PNG/JPEG/PCM WAV plus cleared Ogg test; record provenance. Probe/pin miniaudio/stb_vorbis and restrict decoder/input features before adoption. Silent builds require no missing Qt Multimedia installation; no production art needed | UI/content | Codec and deployment probe |
| Confirm test-device availability | Inventory mac arm64, Windows amd64/arm64, Linux amd64/arm64 devices or volunteered access. Native compile evidence alone is insufficient for GPU/audio support | QA/build-release | M0 risk assessment; full qualification at M9 |

All of these start as inspections or small engineering tasks. Missing paid services are not presumed. If a CI limit would cause a cost, produce a concrete alternative/estimate before asking the user to authorize spending.

## By the first playable, M3

| Need | Minimal useful resource | Cost/access expectation |
| --- | --- | --- |
| Five independent player sessions | Volunteers who can try a 30–45-minute local build; include at least two people unfamiliar with ML and one strategy-game player | User-arranged or explicitly authorized outreach; no automatic invitations or player tracking |
| Visual preference feedback | Review original procedural Office/Lab placeholders and one map/table comparison | No asset purchase needed |
| US boundary inputs | Selected Census state-boundary release or comparably cleared source, reduced to offline geometry; record year/source/transformation | Public download; no API key; agent may obtain during the assigned content task |
| Simple product/research writing review | Check that a novice can distinguish training, evaluation, and deployment | Human feedback; no specialist paid service assumed |

Players may volunteer manually written feedback. Do not embed analytics or add remote reporting to make playtesting convenient. Record only the feedback needed for the development task, with permission; do not commit personal contact details or identifying raw reports.

## Before content lock, M8

| Need | Planned acquisition | Required evidence |
| --- | --- | --- |
| Final location art | Seven main sets plus four contextual sets; existing suitable free/cleared material first, original project art when needed | Per-file creator, source, license, modifications, and local rights evidence |
| Reusable UI symbols | Approximately 60 icons; original QML/vector geometry and/or cleared CC0 packs | Pack and per-derived-file provenance; no copied game UI |
| Music shortlist | Five calm/business atmosphere tracks, two tension/event tracks, and short cues where practical; prefer ENDE's human-created catalog | Exact track pages, author, title, license evidence, modifications/loops, credit text, local files |
| Font decision | System font initially; choose an OFL font only if consistent cross-platform metrics are needed | Actual font license and Reserved Font Name handling if modified |
| AI-concept review | A knowledgeable human review of teaching text and future claims | Identify factual error vs deliberate game abstraction; no endorsement claims |
| Broader accessibility/device feedback | Keyboard-only, high scaling, reduced motion, and screen-reader checks on actual target systems | Observed limitations, resolved blockers, precise support claims |

Music is not downloaded or approved yet. ENDE currently identifies its standard license as CC BY 4.0 and allows game use; preserve each selected track's actual terms and attribution even where the site offers an attribution waiver. The site's catalog includes different creation workflows; select the human-created branch to match the project's no-generated-soundtrack preference. [ENDE license](https://ende.app/en/standard-license), [catalog](https://ende.app/en/search).

## For Content Studio, E1/E2

| Need | When / purpose | Access/cost expectation |
| --- | --- | --- |
| A writable development checkout | E1 opens canonical repository data and shared schemas/registry; a packaged game is not a project | Existing checkout is sufficient; copies/fixtures for destructive test cases |
| Three human authoring sessions | E1b before M4: canvas/forms authoring, inspection, testing and graphical finalization without scripting. E1a's pre-M3 engineering walkthrough is a separate smaller gate | Include a scenario author and someone without scripting experience; user-arranged or explicitly authorized contact, manual feedback only |
| Representative content/impact fixtures | Scope/threshold/positive-negative effects, all lifecycles, dialogue results, external edit conflict and recovery | Engineering/authoring task; no cloud/model service |
| Separate editor artifact/channel naming | E2: independently qualified downloads and exact preview-runtime compatibility | Same Qt/CI tooling; no paid node-editor SDK or new server assumed |
| Version reader and release metadata tests | M0-05/E1A-04/E1B-02: safe internal snapshots then graphical batches use shared game/editor/content identity; later Snap mapping | Small repository tooling; no store access needed until an explicitly scoped publication |

No installation/purchase of the referenced games or URLAlchemist is required to implement this plan. Their concepts inform our own UI. Studio assets/help/palettes and recovery files are excluded from game packages. The later Snap task concerns the game; an editor Snap is a separate future decision.

## Before public distribution, M9/M10

| Need | When | Notes |
| --- | --- | --- |
| Final public title, short description, cover and screenshots | M9 | Working codename is usable internally; check title conflicts before settling branding |
| Itch.io publisher account and chosen project URL | M9 | Existing account may be used. A draft page/release becomes a concrete reviewable task. Do not create or publish during ordinary coding work. |
| Upload method | M9 | Manual upload is sufficient. Automated butler upload optionally uses a narrowly managed credential in the CI secret store, never a committed key or a key pasted into chat. |
| Native clean-device acceptance | M9 | Mac arm64; Windows amd64 and real ARM64; Linux amd64/arm64 with relevant graphics/audio stack. Can use authorized volunteers rather than buying every device. |
| Source and license release bundle | M9 | Matching game source, build instructions, dependency versions, notices, credits, and corresponding-source material appropriate to shipped components |
| Snapcraft publisher access and Ubuntu build/test environment | M10 | Only after primary itch release; Linux amd64/arm64 builds, interfaces/confinement and refresh behavior tested. Credentials entered through supported secure tooling. |
| Explicit publication task | M9/M10 | Development pushes preserve work; public upload/submission is a separate action. |

No paid Apple Developer membership or notarization is required by the baseline. Optional signing/trust improvements can be evaluated later with an actual cost and reason. Windows signing is likewise optional for the initial free release, with accurate installation instructions and real downloaded-artifact tests.

## Not needed

- No game server, VPS, domain, database, accounts system, multiplayer service, or monthly hosting budget.
- No OpenAI/other model-provider key, downloadable real model weights, actual training dataset, remote inference, or GPU-training bill. The game simulates AI development locally.
- No telemetry, analytics, ad-network, crash-upload, remote-config, cloud-save, or in-game-update service.
- No paid GameMaker export, Ren'Py setup, commercial Qt subscription, generated soundtrack, video codec acquisition, or county-by-county dataset for the selected 1.0 plan.

Downloads and updates performed by itch/Snap clients remain outside the game. Their account/store access is publishing/distribution access, not a backend dependency imposed on game code or players.
