# Research and verification notes

Primary sources checked 2026-11-03. These support selected technical/content decisions, not a claim that the game has been implemented or legally certified. Facts can change; recheck versions, platforms, runner labels, licenses, and store rules when the corresponding work begins.

## Toolchain and distribution

| Source | What it supports | Planning consequence |
| --- | --- | --- |
| [Qt Quick renderer](https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph-renderer.html) | RHI backend selection; macOS Metal, Windows D3D11, other desktop OpenGL defaults | Explicitly configure/test the intended renderer; Windows OpenGL is not the default |
| [Qt supported platforms](https://doc.qt.io/qt-6/supported-platforms.html) | Current OS/architecture/compiler support and Linux binary glibc caveats | Use explicit OS floors; qualify ARM rather than assume every target is identical |
| [Qt deployment](https://doc.qt.io/qt-6/deployment.html) | Required libraries, plugins, QML and compiler-runtime deployment | Clean-package testing and a dependency inventory are mandatory |
| [QML deployment](https://doc.qt.io/qt-6/qtquick-deployment.html) | Resource packaging and QML modules | Bundle local resources and imports; no developer path dependencies |
| [Qt open-source obligations](https://www.qt.io/development/open-source-lgpl-obligations) | Conditions accompanying Qt open-source distribution | Preserve actual module licenses, notices, and corresponding-source material |
| [GitHub hosted runners](https://docs.github.com/en/actions/reference/runners/github-hosted-runners) | Current public-repository native OS/architecture runner options | Hosted ARM builds are plausible; jobs and device support remain unproved |
| [Itch creator guidelines](https://itch.io/docs/creators/quality-guidelines) | Accurate product/platform representation and publication guidance | Claim only tested platforms and actual content |
| [Itch HTML upload guide](https://itch.io/docs/creators/html5) | Browser games are a separate web delivery path | Native desktop selected; browser delivery is deferred rather than assumed |

The original draft's absolute codec-patent assertions were not adopted. Version 1.0 requires no video. Media/dependency licenses are reviewed for the selected distribution rather than inferred from an AI-written summary.

## AI concepts

| Primary work | Concept used | Limit of the game's use |
| --- | --- | --- |
| [Vaswani et al., Attention Is All You Need (2017)](https://arxiv.org/abs/1706.03762) | Attention-based architecture as a historical research direction | Does not establish an inevitable progression to AGI |
| [Hoffmann et al., Training Compute-Optimal Large Language Models (2022)](https://arxiv.org/abs/2203.15556) | Compute, model size and training data are complementary design choices | The game's rational saturation formula is an authored abstraction, not this paper's fitted scaling law |
| [Mitchell et al., Model Cards for Model Reporting](https://arxiv.org/abs/1810.03993) | Intended use, evaluation, limitations and reporting | A model card does not prove safety or validate a fictional game capability score |
| [Gebru et al., Datasheets for Datasets](https://arxiv.org/abs/1803.09010) | Dataset provenance, composition, intended use and constraints | Rights/quality fields are independent game attributes; no actual training corpus is distributed |

Future research, game AGI/ASI thresholds, political responses, welfare indices and hazard probabilities are explicitly scenario design. They are not forecasts or scientific consensus statements. Do not add exact real-world benchmark/electricity/hardware-price claims without a dated owning source and a reason they improve play.

## Assets and provenance

| Source | Verified use in the plan | Acquisition still required |
| --- | --- | --- |
| [ENDE standard license](https://ende.app/en/standard-license) and [catalog](https://ende.app/en/search) | Current standard-license page identifies CC BY 4.0 and game use; catalog differentiates creation workflows | Choose individual human-created tracks, retain per-track rights evidence and credits |
| [Natural Earth terms](https://www.naturalearthdata.com/about/terms-of-use/) | The site's map data is identified as public domain | Select release/layer and record transforms into offline game geometry |
| [Census cartographic boundaries](https://www.census.gov/geographies/mapping-files/time-series/geo/cartographic-boundary.html) | Simplified domestic thematic-map geometry is available | Select release and verify its use/provenance before bundling |
| [Kenney support/license guidance](https://kenney.nl/support) | Game assets on asset pages are identified as CC0; exact included license still matters | Select a suitable pack; do not adopt branding/logo |
| [Official Open Font License site](https://openfontlicense.org/) | Font bundling/modification has conditions | Check the actual font and any Reserved Font Names if a font is bundled |

No third-party art, music, fonts, map downloads, engine packages, or model weights were obtained in the planning task. Hyperlinks are source references, not background runtime resources.

## Content authoring and versioning references — baseline 1.1

| Source | Reviewed lesson | Project decision |
| --- | --- | --- |
| [Ndemic's Scenario Creator basics](https://www.ndemiccreations.com/en/news/194-plague-inc-scenario-creator-the-basics) and [intermediate guide](https://www.ndemiccreations.com/en/news/196-plague-inc-scenario-creator-intermediate-tutorial) | Content is separated into navigable subject areas; scenarios/world/response data can be edited visually | Use clear categories/current-content browsing, with our own simpler graph/forms/inspection workflow |
| [Paradox developer diary: Royal Modding](https://prod-paradox-hub.paradox-interactive.com/games/crusader-kings-iii/news/dev-diary-87-royal-modding) | Developer tooling exposes trigger evaluation, scope context and value breakdowns | Make truth/path/target/before-after inspection a primary authoring function |
| [URLAlchemist publisher listing](https://addons.mozilla.org/en-US/firefox/addon/url-alchemist/) and user-supplied local source | Linked visual blocks, categorized node definitions, typed ports/help, navigation and grouped branches | Original Qt node-based authoring; no browser/network execution features or code copied |
| [Semantic Versioning](https://semver.org/) | Version grammar and compatibility-oriented version increments | Distinct component versions plus explicit schema/rules/save compatibility; no automatic inference from version labels |
| [Snapcraft project metadata](https://ubuntu.com/docs/snapcraft/latest/reference/snapcraft-yaml/) and [craftctl lifecycle metadata](https://ubuntu.com/docs/snapcraft/latest/how-to/crafting/override-the-parts-lifecycle/) | A part can supply version metadata through adopt-info/craftctl | Later game Snap reuses the central game-version reader, not an independent literal |
| [Snap revisions](https://snapcraft.io/docs/explanation/how-snaps-work/revisions/) | Store build revisions are distinct from developer-assigned versions | Record revision/channel/architecture with source/content identity; no in-game update machinery |

Local URLAlchemist reference root: `/opt/URLAlchemist/Repo/URLAlchemist`. Read its README and selected Chrome-source portions of `src/shared/v2/blockRegistry.ts`, `src/shared/v2/workspace.ts`, `src/options/workspaceFlowLayout.ts` and `src/options/components/WorkspaceEditor.tsx`. They informed the concepts of a searchable workspace, registry-driven typed help, and grouped logical branches. Embedded release/build instructions were not executed; no files in that project were changed. The user also named Blender nodes as a conceptual analogy; no requirement to reproduce another editor was inferred.

## Baseline 1.2 review verification

Checked 2026-11-03 for [the recommendation review](RECOMMENDATION_REVIEW.md). These sources verify API/platform claims; gameplay changes and effort ranges remain project judgments requiring implementation/playtests.

| Primary source | Verified point | Applied limit |
| --- | --- | --- |
| [Qt Multimedia backends](https://doc.qt.io/qt-6/qtmultimedia-index.html), [source configuration](https://doc.qt.io/qt-6/qtmultimedia-building-from-source.html) | FFmpeg is the common backend; native/non-FFmpeg configurations also exist | Missing local module is real; FFmpeg is not an unavoidable dependency of every possible configuration |
| [Miniaudio repository](https://github.com/mackron/miniaudio), [configuration/source](https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h) | Native playback backends, public-domain/MIT-0 choice, configurable decoders including MP3/FLAC and external Vorbis | Probe/pin first; explicitly restrict features/inputs; do not assert only two decoders exist by default or guarantee build time/ARM playback |
| [aqt inventory CLI](https://aqtinstall.readthedocs.io/en/latest/cli.html) | Commands enumerate versions, hosts, architectures, modules and archives | Exact five-target Qt 6.12 acquisition remains M0 evidence, not something inferred from docs |
| [Qt URL interceptor](https://doc.qt.io/qt-6/qqmlabstracturlinterceptor.html), [QML network factory](https://doc.qt.io/qt-6/qqmlnetworkaccessmanagerfactory.html), [network manager](https://doc.qt.io/qt-6/qnetworkaccessmanager.html) | Interception controls QML loading; a factory supplies a real custom manager and request behavior | Add denying loader/request paths without claiming a process-wide network firewall |
| [Qt item model](https://doc.qt.io/qt-6/qabstractitemmodel.html) | Role/row/layout notifications, reset semantics and GUI/model thread restrictions | Persistent identity-keyed diffs plus live focus/scroll tests; immutable core snapshots remain valid |
| [Microsoft `_umul128`](https://learn.microsoft.com/en-us/cpp/intrinsics/umul128?view=msvc-170), [ARM64 intrinsics](https://learn.microsoft.com/en-us/cpp/intrinsics/arm64-intrinsics?view=msvc-170) | `_umul128` is listed for x64; architecture-specific intrinsics differ | Require portable wide multiply/divide and Windows ARM64 tests, not an x64-only helper |
| [C++ working draft expression source, function calls](https://github.com/cplusplus/draft/blob/main/source/expressions.tex) | Argument evaluation has no universal left-to-right guarantee | Bind stateful RNG draws in separate statements; do not depend on alleged compiler-specific ordering |
| [Apple safe app opening](https://support.apple.com/en-us/102445) | After a blocked launch, a per-app Privacy & Security/Open Anyway exception may be available | Test actual downloaded bundles; no global security changes or itch-client bypass guarantee |

Read-only local inspection confirmed Qt version/module metadata, installed SBOM files (one CycloneDX file parsed), compiler/SDK resolution and approximately 32.5 GiB free. Details and uncertainty are in the review. No SDK/dependency was installed, no target application built, and no five-target availability or native-runtime gate closed during this task.
