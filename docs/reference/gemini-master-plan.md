# Project Glowing Pancake: Master Game Design & Technical Plan

**License:** GPLv3 (Free and Open Source)  
**Repository:** `git@github.com:gnaservicesinc/Project-Glowing-Pancake.git`  
**Local Workspace:** `/opt/AI Startup/Project-Glowing-Pancake`  
**Timeline Scope:** 2014 – 2114 (100-Year Campaign) \+ Endless Sandbox Mode

---

## 1\. Executive Summary & Core Vision

**Project Glowing Pancake** (working title) is a deep business, political, and technological simulation game where the player steps into the shoes of the Founder and CEO of an artificial intelligence startup in 2014\.

Drawing UI and structural inspiration from *Plutocracy*—specifically its clean, room-based navigation, state- and county-level infrastructure management, and systemic political/boardroom negotiations—the game ditches rigid card-based mechanics in favor of a **bounded, open-ended systemic simulation**. Players navigate a 100-year technological arc from early deep learning and convolutional networks to massive foundation models, autonomous agents, AGI, and potentially ASI.

The goal is to teach real-world AI concepts (compute scaling, data curation, alignment vs. capabilities, open-weight vs. proprietary moats) and explore complex socio-economic outcomes without ever feeling like a lecture. Every ethical, technical, and financial shortcut has systemic ripple effects across the company board, public sentiment, electrical grids, and global geopolitics.

---

## 2\. Engine & Technical Stack Decision

Choices that need to be made:
	1. Programming language/tools:
		-A few options are provided, QT is installed in /opt/Qt (curreent mac qt installed locally in the home path /opt/Qt/6.12.0/macos) and then workflows in GitHub can be used.
		- Another option is "GameMaker" found in "/Applications/GameMaker LTS 2026.app" and if needed, a starter project can be created to get it set up for you.
		- Another option is “Renpy,"  found in “ /Applications/renpy-sdk/renpy.app, " and if needed, a starter project could be created to get it set up.
		- Another option is to do something else entirely.

	2. Build system:
		-Set it up to work with meson / ninja or make / cmake  and/ or autotools. Include a make.sh script that when run will just call the relevant tools (autoconfig / a bootstrap script / meson setup, etc., for the system we used) to create a fully optimized release build. 

### Evaluating the Candidates Against Your Requirements

Your project has very specific technical requirements:

1. Must support a `make.sh` script wrapping **CMake / Meson / Ninja** that defaults to an optimized, telemetry-free release build.  
2. Must use **Metal** on macOS (`arm64`) and **OpenGL** on Linux/Windows (`amd64` and `arm64`).  
3. Needs rich UI capabilities (interactive state/county maps, financial ledgers, tech trees, org charts, room scenes) rather than twitch physics.  
4. Must build portably via **GitHub Actions** (to save local disk space) and package cleanly for **itch.io** and **Snapcraft**.

| Engine / Framework | Pros | Cons against Requirements | Verdict |
| :---- | :---- | :---- | :---- |
| **Qt 6.12 (C++20 \+ Qt Quick/QML)** | Native CMake/Ninja integration (`make.sh` is trivial); Qt RHI automatically uses **Metal on macOS** and **OpenGL on Linux/Windows**; unmatched for *Plutocracy*\-style data UIs, maps, and charts; native `arm64`/`amd64` compilation on all 3 OSes \+ Snapcraft. | Requires writing scene transitions and audio triggers in QML/C++ rather than a visual novel script. | **RECOMMENDED (Best Fit)** |
| **GameMaker LTS 2026** | Fast 2D prototyping. | Does not use CMake/Meson natively; macOS IDE cannot locally cross-compile cleanly without complex runners; HTML5 export has quirks; heavy data/table UIs (like *Plutocracy*) are painful to build in GML. | Not Recommended |
| **Ren'Py** | Easy multi-platform packaging for itch.io; great for dialogue. | Python-based (no CMake/Meson build pipeline); weak performance for complex 100-year county-level economic/grid simulations; awkward Snapcraft integration. | Not Recommended |

### Selected Architecture: C++20 Simulation Core \+ Qt 6.12 (Qt Quick / QML)

* **Simulation Engine (`src/core/`)**: Written in pure, standard C++20 with zero UI dependencies. Handles the tick-based simulation (daily/weekly ticks over 100 years), AI competitor logic, economic models, and save/load serialization.  
* **Presentation Layer (`src/ui/`)**: Built with **Qt Quick (QML)** and **Qt Graphs/Canvas**. QML leverages the Qt Rendering Hardware Interface (RHI), which we will explicitly configure to run on **Metal** on macOS and **OpenGL** on Linux and Windows.  
* **Local Development Setup**: Uses `/opt/Qt/6.12.0/macos` for local header verification and lightweight testing, while heavy multi-platform release builds and library bundling (`macdeployqt`, `windeployqt`, `linuxdeploy`) happen in GitHub Actions.

---

## 3\. Core Gameplay Loop & UI Architecture (*Plutocracy*\-Inspired)

Instead of controlling a walking character, the player interacts with the world through stylized, clickable **First-Person Location Sets** and **Strategic Overviews**:

1. **The CEO's Office (Hub)**: View financial summaries, read incoming emails/memos, check the calendar, manage your personal public persona, and review board satisfaction.  
2. **The ML Design & Research Lab**: Configure training runs, allocate compute clusters, select dataset sources, tune alignment guardrails vs. raw capability, and navigate the non-linear Research Tree.  
3. **The Map Room (US State/County & Global View)**:  
   * **US Map (Detailed State & County Level)**: Scout locations for data centers and offices based on county land costs, local electricity rates ($\$/\text{kWh}$), ambient climate (cooling efficiency), fiber-optic backbone access, state tax incentives, and regional disaster risks.  
   * **Global Map (Country & Bloc Level)**: Track international competitors, manage global API distribution, comply with or evade international regulatory bodies (e.g., EU AI regulations, UN summits), and monitor geopolitical tension.  
4. **HR & The Boardroom**: Hire, promote, or fire C-suite executives (Head of Safety, Chief Scientist, VP of Policy, CFO) and engineering tiers. Negotiate equity, handle internal ideological schisms, and survive board votes.  
5. **Capitol Hill & Media Studio**: Lobby lawmakers, testify before Congress, launch targeted advertising campaigns, and conduct high-stakes TV interviews.  
6. **Venture Capital & Banks**: Pitch VCs during seed/Series A–F rounds, negotiate compute-for-equity deals with tech giants, manage debt, or take the company public (IPO).

---

## 4\. Deep Dive: Simulation Systems (2014–2114)

### 4.1 The 100-Year Technological Eras

* **Era 1: The Deep Learning Dawn (2014–2019)**: CNNs, RNNs, LSTMs, early reinforcement learning, and the discovery of the Transformer architecture. Compute budgets are small; academic publishing is the norm.  
* **Era 2: The Scaling & Foundation Era (2020–2028)**: LLMs, multimodal models, diffusion, RLHF, reasoning models, and early agentic workflows. Massive capital requirements, data scraping battles, copyright lawsuits, and the first "Code Red" responses from tech giants.  
* **Era 3: The Autonomous & Industrial Era (2029–2045)**: Embodied robotics, automated scientific discovery, synthetic data dominance, local energy crises (SMRs/nuclear deals for data centers), and structural unemployment triggers.  
* **Era 4: The AGI Threshold (2046–2075)**: Recursive self-improvement, algorithmic breakthroughs beyond transformers, sovereign AI treaties, and intense existential safety vs. acceleration debates.  
* **Era 5: The Post-Singularity Century (2076–2114)**: ASI emergence, space-based/orbital solar compute, global utopia orchestration, corporate hegemony, or survival in a fractured world.

### 4.2 ML Lab, Datasets, and the Non-Linear Tech Tree

Model capability $C$ for a training run is governed by a systemic scaling relation inspired by real-world scaling laws:

$$C(N, D, Q_{\text{arch}}, Q_{\text{data}}) = \alpha \cdot \left( \frac{N}{N_0} \right)^{\beta} \cdot \left( \frac{D}{D_0} \right)^{\gamma} \cdot Q_{\text{arch}} \cdot Q_{\text{data}} \cdot (1 - \tau_{\text{safety}})$$

Where:

* $N$ \= Effective compute (FLOPs) allocated from your hardware clusters.  
* $D$ \= Dataset token volume.  
* $Q_{\text{arch}}$ \= Architectural efficiency multiplier from your non-linear research tree.
* $Q_{\text{data}}$ \= Data quality multiplier (affected by curation, synthetic generation, or licensing).
* $\tau_{\text{safety}}$ \= The "alignment tax" (can be reduced by researching advanced interpretability and alignment techniques).

**Strategic Choices in the Lab:**

* **Data Sourcing**: Public web crawls (cheap, high copyright lawsuit risk), licensed publisher/academic data (expensive, high quality), user interaction logs (privacy backlash risk), or synthetic data (requires high existing model capability to avoid model collapse).  
* **Open vs. Closed Research**: Publishing papers or open-sourcing weights increases **Academic Prestige** (making top researchers cheaper/easier to hire) and **Public Trust**, but boosts the baseline $Q_{\text{arch}}$ of all domestic and international competitors. Keeping IP secret builds a proprietary moat but angers open-source-minded staff.  
* **Guardrails & Specialization**: Prior to AGI, players specialize models across domains (Coding, Biotech/Medicine, Finance, Persuasion/Companion, Defense/Cyber, Robotics) and set policy toggles (e.g., strict enterprise compliance vs. permissive consumer engagement).

### 4.3 Hardware, Energy & Regional Infrastructure

* **Historical Hardware (2014–2026)**: Fictitious equivalents of real hardware eras—from early dual-GPU workstation rigs (2014) to tensor-core server pods (2017–2020), massive H100/B200-style liquid-cooled clusters (2022–2026), and custom ASICs/TPUs.  
* **Speculative Future Hardware (2027–2114)**: Wafer-scale engines, photonic/optical interconnects, neuromorphic processors, cryogenic quantum-hybrid accelerators, and dedicated small modular nuclear reactors (SMRs) or geothermal plants co-located with data centers.  
* **Geography & Disasters**: Building a massive cluster in Northern Virginia gives low latency; building in rural Washington or Quebec gives cheap hydro power; building in California carries seismic/wildfire risks and high power costs; building in Texas risks winter grid freezes or tornadoes. Players must invest in backup generators, cooling redundancy, and geographic replication.

### 4.4 Competitors & The "Code Red" Mechanic

At game creation, the world spawns:

* **$N$ Domestic Startups**: Agile competitors competing for the same VC pool, talent, and compute.  
* **$N$ International Startups**: Operating under different regulatory and subsidy regimes (e.g., state-backed competitors in China, privacy-focused labs in the EU).  
* **$N$ Industry Heavyweights**: Trillion-dollar search, social, and cloud conglomerates. Initially, their bureaucratic inertia keeps their AI deployment slow (despite huge internal research). However, if your startup crosses a **Public Hype or Market Share Threshold** too early, heavyweights trigger a **"Code Red"**, pivoting massive capital and compute into crushing or acquiring you.

### 4.5 Executive Drama, HR, and Public Persona

* **Key Executives**: Your C-Suite has distinct ideological profiles (*Safetyist / Doomer*, *Effective Accelerationist*, *Open-Source Evangelist*, *Corporate Pragmatist*).  
* **Revolts & Whistleblowers**: Pushing capabilities while slashing the safety compute budget can cause your Head of Safety to resign and go on national television, triggering a Congressional subpoena and tanking enterprise trust. Conversely, pausing deployment for months to run safety audits may cause your Board of Directors and Chief Scientist to stage a boardroom coup.  
* **Founder Persona**: Your interviews, lobbying, and publishing choices shape your archetype (*The Pragmatist*, *The Open-Source Hero*, *The Safety Prophet*, *The Techno-Optimist*, *The Ruthless Monopolist*), unlocking unique dialogue, funding sources, and political leverage.

### 4.6 Victory Conditions

Selectable at campaign start (2014–2114) or disabled in Sandbox Mode:

1. **World Monopoly**: Control $\ge 85\%$ of global AI compute/API market share by 2114\.  
2. **World Domination**: Achieve supreme hard or soft power over national and international governing bodies by 2114\.  
3. **World Utopia**: Guide AI deployment to eliminate poverty, solve climate/disease crises, and maintain high global democracy and happiness indices by 2114\.  
4. **Profit Maximization**: End 2114 with the highest total corporate \+ founder net worth, regardless of market share.  
5. **The ASI Race**: Be the first entity on Earth to safely (or unsafely) initialize Artificial Superintelligence (even if a rival reached AGI first).  
6. **Simple Survival**: Keep your company solvent and independent until January 1, 2114\.

---

## 5\. Build System, Packaging & CI/CD Specification

### 5.1 Local Build System (`CMake` \+ `make.sh`)

The repository root will include a `CMakeLists.txt` (supporting Ninja and standard Makefiles) and an executable `make.sh` wrapper script:

* Running `./make.sh` with no arguments configures CMake in **Release Mode** (`-O3 -DNDEBUG`), strips all debugging/telemetry symbols via preprocessor macros (`-DPGP_DEBUG=0 -DPGP_NO_TELEMETRY=1`), compiles the project, and cleans intermediate object files if requested.  
* Running `./make.sh --debug` enables developer diagnostics, assertions, and verbose simulation logging (`-DPGP_DEBUG=1`).

### 5.2 GitHub Actions Multi-Platform Matrix

To adhere strictly to local disk-space constraints, all cross-platform packaging will be executed via `.github/workflows/release.yml`:

* **macOS (`arm64` Apple Silicon)**: Compiles against Qt 6, enforces Metal backend via `QSG_RHI_BACKEND=metal`, bundles frameworks into `.app` / `.dmg` using `macdeployqt` (unsigned/ad-hoc signed for itch.io).  
* **Linux (`amd64` & `arm64`)**: Compiles against Qt 6, sets OpenGL backend, packages standalone portable bundles (AppImage / `.tar.gz` with bundled Qt libs) for itch.io.  
* **Windows (`amd64` & `arm64`)**: Compiles via MSVC, sets OpenGL backend, bundles DLLs via `windeployqt` into portable `.zip` archives for itch.io.  
* **Secondary Distribution (Phase 2\)**: `snapcraft.yaml` configuration for Linux `amd64` and `arm64` once the primary itch.io release is stable.

---

## 6\. Safety, Privacy & Legal Compliance Framework

### 6.1 Content Safety (Strict Adherence)

* **Itch.io TOS & Lawfulness**: Zero unlawful content, hate speech, or gruesome imagery.  
* **Fleeting Consequences**: Catastrophic outcomes (e.g., autonomous weapons misfire, rogue ASI, severe infrastructure failure) are presented cleanly via stylized news tickers, executive memos, and a "Game Over / Post-Mortem" summary screen—never graphic or traumatic prose.  
* **Adult Content Policy**: No NSFW, suggestive, or sexualized text/imagery in the game. The closest mechanical representation is a corporate policy toggle in the ML Lab (*"Allow Lawful Adult Content for Verified Users"*), which affects revenue, payment-processor risk, and brand reputation with brief, humorous, non-descriptive business flavor text.  
* **Substance References**: Any references (e.g., Silicon Valley microdosing tropes) are strictly fleeting, non-descriptive, and never glamorized.

### 6.2 User Privacy & Zero Telemetry

* The game makes **zero network calls** in release builds.  
* State is saved exclusively to local, human-readable or compressed JSON/binary save files in the user's standard OS save directory.  
* All debug logging is stripped at compile time in default builds via `#ifdef PGP_DEBUG`.

### 6.3 Patent-Free Codecs & Asset Provenance

* **Images**: Strictly `.png` (for UI, icons, pixel-accurate graphics) and `.jpg` (for large backgrounds/maps). **Forbidden**: `.heic`, `.avif`, `.jxl` (JPEG XL), or JPEG XS.  
* **Video**: Strictly **MPEG-4 Part 2 Visual** (patent-expired worldwide) if video cutscenes are used, or native QML procedural animations. **Forbidden**: H.265/HEVC, patent-encumbered pools.  
* **AI-Generated Image Metadata**: Any AI-generated visual asset committed to the repo must have embedded EXIF/PNG-tEXt metadata stating: `Generator: AI`, `License: GPLv3`, and `GameContext: <description of asset usage>`.  
* **Music & Credits**: All music tracks will be sourced from **Ende.app** (`https://ende.app/en/search`) and documented in `assets/audio/CREDITS.json`, which dynamically populates the in-game **Credits Screen**.

---

## 7\. Phased Development Roadmap

* **Phase 1: Repository Bootstrap & Build Skeleton**  
  * Initialize `/opt/AI Startup/Project-Glowing-Pancake` structure, `.gitignore`, `CMakeLists.txt`, `make.sh`, and `AGENTS.md`.  
  * Create the minimal Qt 6.12 window enforcing Metal (macOS) / OpenGL (Linux/Win) and verify GitHub Actions workflow.  
* **Phase 2: The Core Simulation Engine (Headless C++20)**  
  * Implement the date/tick loop (2014–2114), company ledger, hardware inventory, and scaling-law ML training formulas.  
  * Implement save/load serialization and unit tests.  
* **Phase 3: UI Shell & Room Navigation (*Plutocracy* Style)**  
  * Build the QML navigation bar, CEO Office, ML Lab, Finance screen, and Credits screen.  
* **Phase 4: Geography, Competitors & "Code Red"**  
  * Build the interactive US State/County Map (power costs, cooling, disasters) and Global Map.  
  * Implement AI competitor startups and Heavyweight "Code Red" state machines.  
* **Phase 5: Politics, HR Drama & Media Systems**  
  * Implement C-suite loyalty/whistleblowing, Congressional hearings, lobbying, and the 6 Victory Conditions.  
* **Phase 6: Polish, Audio Integration & Distribution**  
  * Integrate Ende.app soundtrack, verify asset metadata/codec compliance, run full itch.io release builds, and prepare Snapcraft packaging.

---

## 8\. Agent Operating Instructions (`AGENTS.md` Blueprint)

The following rules should be placed directly into `/opt/AI Startup/Project-Glowing-Pancake/AGENTS.md` so any coding agent working on your machine obeys your environment constraints:

```markdown
# AGENTS.md - Workspace & Operational Directives

1. **Workspace Boundary**:
   - All work MUST remain strictly inside `/opt/AI Startup`.
   - All persistent source code, assets, and documentation MUST reside inside the Git repository at `/opt/AI Startup/Project-Glowing-Pancake`.

2. **Aggressive Disk Space Hygiene**:
   - Local Qt installation is located at `/opt/Qt/6.12.0/macos`.
   - Do NOT leave build artifacts, object files, or temporary test binaries on disk after verifying a build. Always clean build directories (`rm -rf build/` or `./make.sh --clean`) before ending a session.
   - Heavy multi-platform release builds and library bundling must be delegated to GitHub Actions workflows.

3. **Git Persistence Mandate**:
   - Unpushed local files may be wiped between agent sessions if disk space runs low.
   - Before concluding ANY task or sub-task, you MUST stage, commit, and push your work to `git@github.com:gnaservicesinc/Project-Glowing-Pancake.git`.
   - Verify `git status` is clean and `git push` succeeded before finishing.

4. **Build & Telemetry Standards**:
   - `./make.sh` MUST build a fully optimized Release binary (`-O3 -DNDEBUG`) with all debug/telemetry code stripped by default.
   - Debug builds require passing `./make.sh --debug`.
   - Zero network telemetry or user tracking code may ever be introduced.

5. **Asset & Codec Compliance**:
   - Only `.png`, `.jpg`, and patent-free MPEG-4 Visual video assets are permitted. Never commit HEIC, HEVC, JPEG XL, or JPEG XS files.
   - Every AI-generated image must include embedded metadata describing its AI origin and in-game purpose.
   - All audio tracks from ende.app must be logged in `assets/audio/CREDITS.json` and displayed in the UI Credits view.
```