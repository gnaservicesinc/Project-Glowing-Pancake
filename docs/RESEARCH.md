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
