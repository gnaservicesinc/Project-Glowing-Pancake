# Project Glowing Pancake — Plan Review & Architectural Recommendations

**Reviewed Baseline:** Planning Baseline 1.1 (`a218fc6`, 2026-11-03)  
**Repository:** `/opt/AI Startup/Project-Glowing-Pancake`  
**Document Location:** `/opt/AI Startup/Gemini4Recommendations.md`  

---

## Executive Summary

The **Baseline 1.1** documentation in `/opt/AI Startup/Project-Glowing-Pancake` is exceptionally well-structured. Its core architectural decisions—separating the pure C++20 deterministic simulation (`src/core/`) from the Qt Quick/QML bridge (`src/bridge/`), enforcing integer-only authoritative math, sharing one typed flow compiler/runtime between the game and **Content Studio** (`pgp-editor`), maintaining strict offline privacy, and bounding the first playable vertical slice (`M3`, 2014–2017)—provide a strong foundation.

However, a deep cross-document audit of the specifications, example JSON contracts, roadmap dependencies, and the actual local development environment (`/opt/Qt/6.12.0/macos`) revealed **several critical technical traps, mathematical edge cases, schedule bottlenecks, and gameplay gaps** that could force costly mid-development redesigns if not addressed early.

### Top 7 High-Impact Findings (Quick Index)

1. **[Environment Blocker — M0] `Qt6Multimedia` is not installed in `/opt/Qt/6.12.0/macos` (and pulls in FFmpeg in Qt 6):** Local builds requiring `Qt6::Multimedia` will fail immediately, and only 12 GB of disk space remains free. Switching audio to a tiny embedded single-header backend (`miniaudio` + `stb_vorbis`) or making `Qt6Multimedia` strictly optional solves local builds, eliminates FFmpeg dynamic library bloat, and guarantees only PCM WAV and Ogg Vorbis decoders exist in the binary (Section 1.1).
2. **[Math & Overflow — M1/M2] 64-bit integer overflow in utility squaring, multi-factor basis points, and 500-year inflation:** Squaring customer utility (`0–10,000` $\rightarrow 10^8$) and multiplying by global segment budgets in cents ($10^{14}$) reaches $10^{22}$, overflowing `int64_t` ($\approx 9.22 \times 10^{18}$). Chained basis-point multiplications in `effective_data_j` and `capability_j` also risk either integer truncation or `int64_t` overflow without a portable 128-bit intermediate helper (Section 2.1).
3. **[Simulation Spec Gap — M2/M3] Missing inference-compute consumption formula and missing "Infrastructure Provider" revenue channel:** `CONTENT_AND_ASSETS.md` promises an *"infrastructure provider"* strategic path, but `GAME_DESIGN.md` and `SIMULATION_SPEC.md` only define model-based product channels and omit both wholesale compute leasing and the formula converting delivered customer tasks into daily inference `milli_CU` (Section 2.3).
4. **[Schedule Bottleneck — E1/M3] Full 2D Node Canvas (`E1`) blocks the First Playable (`M3`) for 4–7 weeks just to author 8 events:** Requiring the complete graphical node-wiring canvas and a 3-person author study (`E1-06`) before testing whether the 3-year gameplay loop (`M3`) is fun puts UI tool polish on the critical path ahead of core gameplay validation. Splitting `E1` into **E1a** (Shared runtime + Library + Form/Outline editor + Before/After Inspector) before `M3` and **E1b** (Interactive 2D Node Canvas + Batch Finalizer) alongside `M3` saves 3–5 weeks and avoids reworking editor nodes after `M3` playtesting (Section 3.1).
5. **[Flow Runtime Redesign Risk — M1-06] Numbered `Sequence` fan-out with delays/dialogues requires a continuation stack, not a single `current_node`:** Because branch 1 of a `Sequence` can contain a multi-day `Delay` or branching `Dialogue` before branch 2 runs, a flat `current_node` field cannot resume execution across save/load boundaries (Section 3.2).
6. **[UX Trap — M7] Late-game "Mathematically Dead Campaign" in sustained 5- and 10-year victory windows:** Because `World Utopia` requires meeting all thresholds for the final 120 monthly checks (2104–2114), a single dip in 2105 makes victory impossible while forcing the player to play 9 more in-game years to reach the 2114 evaluation screen (Section 4.4).
7. **[QML Performance & Accessibility Trap — M1-03/M1-05] Pushing 10 Hz immutable snapshots into QML views:** Replacing models or calling `beginResetModel()` at 10 Hz while running destroys QML table delegates, resets scroll bars, and drops keyboard focus unless `src/bridge/` diffs snapshots into persistent `EntityId`-keyed `QAbstractItemModel`s (Section 5.1).

---

## 1. Immediate Environment & Toolchain Realities (Preventing `M0` Delays)

A read-only inspection of the local environment alongside `docs/TECHNICAL_PLAN.md`, `docs/NEEDS_AND_ACCESS.md`, and `docs/BACKLOG.md` identified four concrete issues to handle in `M0`.

### 1.1 `Qt6Multimedia` Is Absent Locally & Introduces FFmpeg Codec Complexity
- **Where in Plan:** `docs/TECHNICAL_PLAN.md` (Section 9, line 153), `docs/BACKLOG.md` (`M0-03`), `docs/CONTENT_AND_ASSETS.md` (Section "Audio direction", line 67).
- **The Issue:**
  1. Inspection of `/opt/Qt/6.12.0/macos` confirms that **only** `qtbase`, `qtdeclarative`, `qtdoc`, `qtimageformats`, `qtsvg`, `qttools`, and `qttranslations` are installed. **`Qt6Multimedia` (`qtmultimedia`) is not installed.**
  2. Free disk space on `/` is currently **12 GB (99% full)**, and `AGENTS.md` strictly forbids downloading large SDK extensions locally.
  3. Furthermore, in Qt 6.5+, `QtMultimedia` defaults to an **FFmpeg (`libavcodec`/`libavformat`) backend** across desktop platforms. Bundling Qt's multimedia plugins in CI (`macdeployqt` / `windeployqt` / Linux `.tar.gz`) pulls in large FFmpeg shared libraries containing hundreds of audio/video codecs—directly conflicting with the project's goal of avoiding complex worldwide codec patent/license audits (`docs/TECHNICAL_PLAN.md`, line 153).
- **Recommendation (Avoid Redesign in `M0-03`):**
  - **Primary Path:** Implement local audio in `src/app/` using **`miniaudio`** (single header, Public Domain / MIT-0, native CoreAudio on macOS, WASAPI on Windows, ALSA/PulseAudio/PipeWire on Linux) paired with **`stb_vorbis`** (single file, Public Domain / MIT, Ogg Vorbis decoding).
  - **Why this is cleaner:**
    - Compiles in <2 seconds with zero external SDK dependencies on all 5 target architectures (`macos-arm64`, `linux-amd64`, `linux-arm64`, `windows-amd64`, `windows-arm64`).
    - Works immediately on the local macOS machine without modifying `/opt/Qt/6.12.0/macos`.
    - Physically contains **only** PCM WAV and Ogg Vorbis decoders—making it impossible to ship unreviewed video/audio codecs.
    - Exposes a tiny C++ `AudioController` singleton to QML (`playCue()`, `setMusicTrack()`, `setMusicVolume()`, `setEffectsVolume()`, `setMuted()`).

### 1.2 Local Compiler Ambiguity (`/usr/local/bin/clang++` vs. Apple Clang)
- **Where in Plan:** `docs/NEEDS_AND_ACCESS.md` (line 23), `docs/BACKLOG.md` (`M0-01`, `M0-02`).
- **The Issue:** On this workstation, default `PATH` resolves `clang++` to `/usr/local/bin/clang++` (**LLVM Clang 23.1.3**), whereas Xcode's toolchain at `/usr/bin/clang++` is **Apple Clang 21.0.0** (`MacOSX.sdk`). When building Qt 6.12 applications on macOS, using a non-Xcode LLVM binary without explicit `--sysroot` and standard-library rpath flags can cause CMake to link against `/usr/local/lib/libc++.dylib` while `/opt/Qt/6.12.0/macos` frameworks link against `/usr/lib/libc++.1.dylib`, leading to runtime ODR/rpath failures or `macdeployqt` errors.
- **Recommendation:**
  - In `CMakePresets.json` and `./make.sh` (`M0-02`), explicitly resolve the macOS compiler via `xcrun --find clang++` (or `/usr/bin/clang++`) and `xcrun --show-sdk-path` for Qt builds, while optionally using `/usr/local/bin/clang++` as an additional compiler matrix check for `--core-only`.

### 1.3 GitHub Actions Qt 6.12 Availability on `linux-arm64` and `windows-arm64`
- **Where in Plan:** `docs/TECHNICAL_PLAN.md` (Section 2, lines 25–37), `docs/BACKLOG.md` (`M0-04`).
- **The Issue:**
  - Ubuntu 24.04 (`noble`) `apt` repositories provide **Qt 6.4.2**, not Qt 6.12.0.
  - Community CI actions such as `jurplel/install-qt-action` (`aqtinstall`) fetch prebuilt Qt archives from `download.qt.io`, which has historically had gaps or required cross-host archive configurations for `linux_gcc_arm64` and `win64_msvc2022_arm64` on newest Qt point releases.
- **Recommendation:**
  - During `M0-01`, query `aqtinstall list-qt` for all five target triplets before writing `M0-04` workflows.
  - Keep QML imports and C++ bridge code compatible with **Qt 6.4+ through 6.12** (e.g., standard `QtQuick`, `QtQuick.Controls`, `QtQuick.Layouts`, `QtQuick.Shapes` without relying on 6.12-exclusive APIs). That way, if `download.qt.io` lacks a prebuilt `6.12.0` binary for `linux-arm64`, the Linux CI job can build cleanly against Ubuntu 24.04's native `qt6-base-dev` / `qt6-declarative-dev` packages with zero code changes.

### 1.4 Leveraging Existing Local Qt SBOMs & Enforcing Offline Execution in Code
- **Where in Plan:** `docs/TECHNICAL_PLAN.md` (Section 7, line 116; Section 9, line 155), `docs/BACKLOG.md` (`M0-01`, `M0-03`, `M8-03`).
- **The Issue & Opportunity:**
  1. `/opt/Qt/6.12.0/macos/sbom/` already contains machine-readable SPDX/CycloneDX JSON files (`qtbase`, `qtdeclarative`, `qtimageformats`, `qtsvg`, etc.) listing exact module licenses (`LGPL-3.0-only`, `GPL-3.0-only`, bundled `libpng`, `libjpeg`, `freetype`, `harfbuzz`). `M0-01` should parse these directly rather than building a manual inventory from scratch.
  2. `libQt6Qml` links against `Qt6Network` by design. To guarantee at the engine level that no QML component or asset path ever initiates a network request (`docs/TECHNICAL_PLAN.md`, line 116):
- **Recommendation:**
  - In `M0-03`, install a custom `QQmlAbstractUrlInterceptor` and a custom `QNetworkAccessManagerFactory` on `QQmlApplicationEngine` in `src/app/` that rejects any URL scheme other than `qrc:` and approved local file paths, immediately failing a test assertion if any `http:`/`https:`/`ws:` request is ever constructed.

---

## 2. Simulation Math, Units & Accounting Fixes (Preventing `M1`–`M3` Rework)

Several formulas and accounting rules in `docs/SIMULATION_SPEC.md` contain unit ambiguities or integer overflow hazards that will surface during `M1`–`M3` implementation and `M8` 500-year endurance runs.

### 2.1 `int64_t` Intermediate Overflows & Basis-Point Truncation
- **Where in Plan:** `docs/SIMULATION_SPEC.md` (Section 1, lines 13–24; Section 4, lines 94, 112; Section 5, lines 122–132).
- **The Issues:**
  1. **Customer Utility Squaring × Segment Budget Overflow (`SIMULATION_SPEC.md`, line 112):**
     The spec states: *"Weight allocation by the square of positive utility above the reservation threshold."*
     If `utility` is on the standard `0–10,000` scale, `weight_p = utility^2` reaches $10^8$ (or $10^9$ across summed products). When allocating a global segment's annual/monthly budget stored in cents (e.g., $\$1\text{ trillion} = 10^{14}\text{ cents}$ in mid-to-late game), computing `segment_budget_cents * weight_p / total_weight` produces an intermediate product of $10^{14} \times 10^8 = 10^{22}$, which overflows signed 64-bit integers (`INT64_MAX` $\approx 9.22 \times 10^{18}$) by three orders of magnitude.
  2. **Chained Basis-Point Multiplications in `effective_data_j` and `capability_j` (`SIMULATION_SPEC.md`, lines 126–132):**
     ```text
     effective_data_j = sum(usable_unique_units_i * quality_i * relevance_ij * coverage_ij)
     capability_j = min(research_ceiling_j, architecture_baseline_j + training_gain_j * bottleneck_j * recipe_fit_j)
     ```
     Here, `quality_i`, `relevance_ij`, and `coverage_ij` are three basis-point values (`0–10,000`). Dividing by `10,000` after each step causes severe step-function quantization on small datasets, while multiplying `quality * relevance * coverage` ($10^{12}$) before dividing overflows `int64_t` whenever `usable_unique_units_i > 9,223,372`.
  3. **`consumed_CU` vs. `milli_CU` Unit Ambiguity (`SIMULATION_SPEC.md`, line 11 vs. line 128):**
     Line 11 defines the stored unit of Capacity as **integer milli-compute-units per day (`milli_CU`)**, and `docs/examples/slice-balance.example.json` (line 31) stores `3,600,000` (`total_milli_CU`) for a 3,600 CU training run. However, line 128 writes `sat(consumed_CU, compute_scale_j)`. If `consumed_CU` is passed in `milli_CU` while a content author enters `compute_scale_j` in whole `CU` (or vice versa), saturation speeds will be off by $1,000\times$.
- **Recommendations:**
  1. **128-Bit Intermediate Checked Math in `M1-01`:** Implement `mul_div_round_half_up(int64_t a, int64_t b, int64_t denom)` and `mul3_bp_round_half_up(int64_t val, Bp a, Bp b, Bp c)` using `__int128` on Clang/GCC and `_mul128` / `_umul128` on MSVC so `a * b / denom` never overflows internally and only checks that the final quotient fits in `int64_t`.
  2. **Strong Value Types in C++20:** Wrap raw `int64_t` in zero-cost strong types (`Cents`, `MilliCU`, `BasisPoints`, `PartsPerMillion`, `OrdinalDay`) so passing `MilliCU` to a function expecting `Cents` or `BasisPoints` fails at compile time.
  3. **Explicit Naming in Schemas:** Rename `compute_scale_j` in `SIMULATION_SPEC.md` to `compute_scale_milli_CU` so both code and JSON schemas use `milli_CU` everywhere.

### 2.2 Constant-2014 Cents vs. Nominal Inflation Drift Over 100–500 Years
- **Where in Plan:** `docs/SIMULATION_SPEC.md` (Section 4, line 94; Section 8, line 186; Section 9, line 196) and `docs/examples/trigger-impact.example.json` (line 11).
- **The Issue:**
  - `SIMULATION_SPEC.md` (line 94) states: *"Nominal prices use a world price index, initialized to 10,000 in 2014. Reports and the profit objective convert with the saved index."*
  - Meanwhile, authored content flows (such as `trigger-impact.example.json`, line 11: `"fact": "company.cash_cents", "op": "gte", "value": 5000000` and `supplier-event.example.json`, line 18: `"company.monthly_compute_bill_cents" >= 2000000`) use fixed integer cent literals!
  - If the simulation inflates nominal prices over a 100-year campaign (or a 500-year sandbox where $2.5\%$ annual inflation multiplies nominal prices by $232,000\times$), every fixed-cent trigger threshold and fixed-cent grant/penalty in Content Studio becomes meaningless in later decades, and 500-year nominal balances risk overflowing `int64_t`.
- **Recommendation:**
  - Keep **all authoritative simulation balances, budgets, and authored content thresholds in Constant 2014 Cents (Real 2014 Dollars)** internally.
  - If macroeconomic inflation is modeled, apply it only as (a) an optional UI display toggle ("Nominal Era Dollars" vs. "2014 Constant Dollars") and (b) a real-value decay factor on unindexed cash holdings or fixed-rate debt. This keeps authored triggers valid across any era, prevents 500-year overflow, and makes financial comparisons across a century effortless for the player.

### 2.3 Century-Scale Compute Scaling vs. "Scenario-Normalized `CU/day`"
- **Where in Plan:** `docs/SIMULATION_SPEC.md` (Section 1, line 11; Section 4, lines 100–106; Section 5, line 128), `docs/GAME_DESIGN.md` (Section 8, line 139).
- **The Issue:**
  - `SIMULATION_SPEC.md` defines Capacity as `milli-compute-units per day`, described as *"a scenario-normalized reference, not advertised FLOPS"*.
  - Over 2014–2114 (18 hardware families across reference and speculative eras), if `CU/day` per dollar grows by real-world hardware ratios ($10^6\times$ to $10^9\times$), early numbers (`100 CU/day`) become `100,000,000,000 CU/day` in late eras. Conversely, if `CU/day` is flatly normalized so every era's hardware outputs ~`100–1,000 CU/day`, then 2014 workstation GPUs never become obsolete for 2035 foundation models.
- **Recommendation:**
  - Use a **bounded logarithmic-feel game progression** across the 18 hardware families (e.g., total per-rack output scaling by roughly $1\times \rightarrow 1,000\times$ over the century rather than $10^9\times$), paired with explicit **Hardware Architecture / Interconnect Tiers (`Tier 1` through `Tier 6`)**.
  - Require recipes of Scale Class $K$ to run on hardware of at least `Interconnect/Memory Tier >= K` (which `GAME_DESIGN.md`, line 107 already hints at: *"The preview flags memory/interconnect incompatibility"*).
  - Allow legacy `Tier 1–2` hardware to continue running older specialist inference models cheaply until physical maintenance/power costs exceed newer hardware, creating a realistic and intuitive hardware upgrade lifecycle.

### 2.4 Missing Inference Compute Formula & Missing "Infrastructure Provider" Channel
- **Where in Plan:** `docs/SIMULATION_SPEC.md` (Section 3, line 84; Section 4, lines 100–116), `docs/GAME_DESIGN.md` (Section 5, lines 78–85), `docs/CONTENT_AND_ASSETS.md` (line 32), `docs/examples/slice-balance.example.json` (lines 35–39).
- **The Issue (Two Connected Gaps):**
  1. **No formula links delivered customer tasks to inference `milli_CU`:**
     `slice-balance.example.json` shows `100,000 milli_CU/day` leased, `60,000 milli_CU/day` used by training, and an enterprise contract paying `$25,000/month` with `$5,000/month` incremental serving/support cost—but neither `slice-balance.example.json` nor `SIMULATION_SPEC.md` defines how many `milli_CU/day` that contract (or discretionary market demand) consumes from the remaining `40,000 milli_CU/day` pool!
  2. **"Infrastructure Provider" strategy lacks a revenue mechanism:**
     `docs/CONTENT_AND_ASSETS.md` (line 32) states: *"Every strategic path needs content: profitable specialist, open ecosystem, frontier lab, public-service partnership, **infrastructure provider**, and authority-seeking institution."* Yet all six product channels in `GAME_DESIGN.md` (lines 78–85) sell a **trained model**. A player who builds massive power-efficient data centers in Washington or Texas currently has no mechanical way to lease excess `CU/day` capacity to other companies or enterprise customers.
- **Recommendations:**
  1. **Explicit Inference Compute Cost (`inference_milli_CU_per_task`):** Every deployed model recipe defines `inference_milli_CU_per_task` (or per 1,000 tasks) and `non_compute_support_cents_per_task`. Researching *Efficient Serving* (`SIMULATION_SPEC.md`, line 150) reduces `inference_milli_CU_per_task`, directly increasing how many customer tasks a company can serve on finite hardware and improving gross margin.
  2. **Add Wholesale Compute / Cloud Hosting Contracts:** Allow the player to offer unreserved owned-site capacity (`milli_CU/day`) to the market or via B2B infrastructure contracts (`Lease Out Capacity`). This fulfills the promised *Infrastructure Provider* strategy and creates a high-stakes strategic dilemma: leasing your clusters to rivals generates steady cash flow to pay off data-center debt, but accelerates your competitors' training runs!

### 2.5 Customer Demand Allocation Convergence & Stateful Switching Costs
- **Where in Plan:** `docs/SIMULATION_SPEC.md` (Section 4, lines 109–116).
- **The Issues:**
  1. **Stateless `switching_cost`:** Line 112 lists `switching_cost` as a component of product `p`'s utility in segment `s`. However, if utility is evaluated statelessly each day/month without knowing which product the segment was already using, `switching_cost` acts as a uniform penalty rather than customer inertia/stickiness.
  2. **Dual-Constraint Water-Filling:** Line 114 requires capping allocation simultaneously by **customer segment budget (cents)** and **product serving capacity (tasks)**, redistributing unserved demand in stable rounds. Because different products charge different prices per task unit, allocating tasks by utility weight can exceed the segment's dollar budget, while allocating dollars by utility weight leaves fractional task remainders.
- **Recommendations:**
  1. **Track `installed_task_share[region][segment][product]`:** Give each product an incumbency utility bonus on its existing installed base in that segment, or cap monthly discretionary share migration by a channel-specific `max_monthly_churn_bp` (e.g., low churn for *B2B Contracts* and *Licensed On-Premise*; high churn for *Consumer App* and *Developer API*).
  2. **Define a 2-Constraint Deterministic Allocation Algorithm in `SIMULATION_SPEC.md` before `M2-03`:**
     - **Step 1 (Eligibility & Effective Price Cap):** Filter products meeting legal/technical requirements and `price_per_task <= segment_max_wtp_per_task`.
     - **Step 2 (Budget-Normalized Task Demand):** In each round (max 8 rounds), split remaining segment task demand `D_rem` proportionally to $w_p = \text{utility}_p^2$. If $\sum_p (\text{tasks}_p \times \text{price}_p) > \text{Budget}_{\text{rem}}$, scale all $\text{tasks}_p$ in that segment down uniformly by $\lfloor \text{Budget}_{\text{rem}} / \sum (\text{tasks}_p \times \text{price}_p) \rfloor$.
     - **Step 3 (Cross-Segment Capacity Contention):** If a product `p` is oversubscribed across multiple segments/regions relative to its available serving capacity, allocate its capacity across segments according to the product's configured priority policy (e.g., *Contract First $\rightarrow$ Highest Margin First $\rightarrow$ Proportional*), then return unserved tasks to the next water-filling round.

### 2.6 Daily Update Order vs. Paused Command Execution & Misclick Refunds
- **Where in Plan:** `docs/SIMULATION_SPEC.md` (Section 2, lines 29–42), `docs/GAME_DESIGN.md` (Section 6, line 107).
- **The Issue:**
  - `SIMULATION_SPEC.md` (line 38) stops at an auto-pause boundary at the **end of Phase 8** (after incrementing the date to `t`, before Phase 1 and Phase 2 of day `t`).
  - However, scheduled deliveries, new laws, and due opening content flows execute in **Phase 2** (`SIMULATION_SPEC.md`, line 30), and end-of-day reaction triggers from day `t-1` queue their work for the next opening boundary (line 38). If the game pauses before Phase 2 runs, the player does not see day `t`'s morning events/deliveries until they unpause!
  - Furthermore, line 42 allows commands to drain immediately while paused, and `GAME_DESIGN.md` (line 107) states that upfront payments (like a `$12,000` data license when starting training) are not refunded on cancellation. If a player misclicks "Start Training" while paused and cancels 2 seconds later without advancing a single day, losing `$12,000` feels punitive.
- **Recommendations:**
  1. **Set the Daily Pause Boundary Between Phase 2 and Phase 3 ("Morning Briefing State"):** When stepping from `t-1` to `t`, increment the date, execute morning deliveries and opening content flows (`Phase 2`), publish the snapshot, and pause if any morning event/alert requires attention. Player commands issued while paused validate and commit before `Phase 3` (staff/compute assignment and work execution) runs.
  2. **Same-Day (`0` Elapsed Days) Grace Cancellation:** Allow orders placed on day `t` while paused (training start, hardware order, hiring target) to be cancelled with a **100% refund while the game is still on day `t` (`elapsed_days == 0`)**. Once the simulation steps to `t+1`, normal non-refundable sunk-cost rules apply.

---

## 3. Content Studio (`pgp-editor`) & Flow Runtime Architecture

The decision to share one typed compiler, registry, and deterministic runtime between the game and Content Studio (`docs/CONTENT_EDITOR.md`, `docs/SIMULATION_SPEC.md` Section 2a) is excellent. To prevent `M1-06` and `E1` from becoming a multi-month bottleneck, four architectural refinements are recommended.

### 3.1 Critical Path Optimization: Decouple 2D Node Canvas Polish from `M3` First Playable
- **Where in Plan:** `docs/ROADMAP.md` (lines 24–25, 37–53), `docs/BACKLOG.md` (`E1-01` through `E1-06`, `M3-02`).
- **The Issue:**
  - In the current dependency graph (`M2` $\rightarrow$ `E1` [4–7 weeks] $\rightarrow$ `M3` [4–6 weeks]), the team cannot playtest the 3-year vertical slice (`M3`, which only needs **8 events**, 6 research nodes, and 4 datasets) until the entire standalone graphical node-canvas editor (`E1-02`), A/B visual diff tool (`E1-03`), journaled revision finalizer (`E1-05`), and 3-human author usability study (`E1-06`) are 100% finished.
  - Building a custom QML node-graph canvas (bezier/orthogonal wire routing, box collision/auto-layout, group drag, undo/redo) is one of the most UI-intensive tasks in the entire project. Putting all of it *before* `M3` delays discovering whether the core business/ML loop is fun, and any simulation adjustments from `M3-04` playtesting will force rework in `E1`.
- **Recommendation (Split `E1` into `E1a` and `E1b`):**
  - **`E1a` (Before `M3`, ~2–3 weeks):** Build the shared flow compiler/runtime (`M1-06`), the separate `pgp-editor` shell with the **Content Library**, the **Complete Outline/Forms Editor** (which `CONTENT_EDITOR.md`, line 98 already requires as a 1:1 equivalent of the graph!), and the **Trigger/Effect Before-After Inspector** (`E1-01`, `E1-03`, `E1-04`). Authors can create, inspect, and validate all 8 slice events without touching raw JSON or scripts.
  - **`E1b` (Parallel with `M3` / Gate before `M4`):** Build the 2D visual node-box canvas (`E1-02` canvas view), batch revision finalizer (`E1-05`), and run the 3-author usability gate (`E1-06`).
  - Because the Graph, Outline, and Forms views edit the exact same in-memory document model (`CONTENT_EDITOR.md`, line 98), splitting `E1a` and `E1b` requires zero throwaway code while pulling the first playable slice (`M3`) forward by 3–4 weeks.

### 3.2 Flow Execution State Needs a Bounded Continuation Stack (`M1-06` / `M1-04`)
- **Where in Plan:** `docs/SIMULATION_SPEC.md` (Section 2a, lines 58, 60), `docs/CONTENT_EDITOR.md` (Section 5, line 94).
- **The Issue:**
  - `CONTENT_EDITOR.md` (line 94) specifies: *"Numbered fan-out compiles to a structured sequence with an explicit continuation: finish output 1 (including any player choice/delay), then output 2, then continue."*
  - If Output 1 of Sequence `S1` leads to a `Dialogue` or a `Delay (14 days)` (or a nested Sequence `S2`), and the player saves the game while waiting on Output 1, a flow instance that only stores `current_node` (`SIMULATION_SPEC.md`, line 60) will not know that after Output 1 reaches its `end` node, execution must return to `S1` Output 2!
- **Recommendation:**
  - Represent each active flow instance's execution pointer in `M1-06` and `.pgpsave` (`M1-04`) as a **bounded continuation stack (`max_depth = 8`)**:
    ```json
    "execution_stack": [
      { "sequence_node": "seq_main", "next_branch_index": 1 },
      { "current_node": "dialogue_followup" }
    ]
    ```
  - Additionally, add a **Content Studio linter warning** whenever an author puts a long `Delay` inside branch 1 of a `Sequence` that has subsequent branches, asking whether they intended to delay branch 2 or meant to use an explicit `schedule_flow` node instead.

### 3.3 Synthetic Lifecycle Harness for Continuous Modifiers in `E1-03`
- **Where in Plan:** `docs/CONTENT_EDITOR.md` (Section 6, line 118), `docs/BACKLOG.md` (`E1-03`).
- **The Issue:**
  - `CONTENT_EDITOR.md` (line 118) requires the Studio inspector to *"Preview entry, a later active tick, and exit for continuous modifiers, demonstrating no double application and correct cleanup."*
  - However, on a copied test fixture state (e.g., `workload_stress_basis_points = 7500`, where exit requires `<= 5000`), simply stepping the simulation forward 1 day will not trigger the exit condition unless something lowers stress below `5000`.
- **Recommendation:**
  - Implement a dedicated **`PreviewContinuousLifecycle(flow_id, fixture_state)`** trace mode in `M1-06`:
    1. **Step A (Entry):** Evaluate entry trigger on copied state and apply `entry_node` $\rightarrow$ record `Entry Delta` (`research_coordination_basis_points: -500`).
    2. **Step B (Active Tick):** Advance 1 day while condition holds $\rightarrow$ verify `Active Tick Delta == 0` (proving no per-tick drift or double application).
    3. **Step C (Synthetic Exit):** Apply the exit condition override (or invoke `exit_node` directly on the copied state) $\rightarrow$ verify `Post-Exit Value == Pre-Entry Baseline` (proving clean removal and no percentage-inversion rounding drift).

### 3.4 Harmonizing Example JSON Schemas Before Implementing `M1-02`
- **Where in Plan:** `docs/examples/supplier-event.example.json`, `docs/examples/flow-lifecycles.example.json`, `docs/examples/trigger-impact.example.json`.
- **The Issue:**
  Comparing the three flow/trigger example files reveals minor structural inconsistencies that should be unified when writing the `M1-02` schema validator:
  1. **Target Binding Location:** `trigger-impact.example.json` places `"target_binding": "self.compute_lease"` at the top level and a single `"effect": {...}` object, whereas `supplier-event.example.json` and `flow-lifecycles.example.json` use `"effects": [ ... ]` inside `"kind": "effect"` nodes with an implicit `"self"` target.
  2. **Hardcoded Calendar Anchor on Interval Flows:** `flow-lifecycles.example.json` (line 93) uses `"anchor_date": "2014-02-01"`. If an interval flow is attached to a company founded after 2014 or a custom scenario, a fixed date anchor requires manual adjustment.
  3. **Viewport State vs. Committed Node Layouts:** `CONTENT_EDITOR.md` (line 98) stores layout files in `authoring/layouts/`. If pan/zoom camera state or unrounded float coordinates are written to `authoring/layouts/`, every time an author opens and looks at a graph, Git will show dirty layout files.
- **Recommendations:**
  - Require every item in an `"effects": [...]` array to carry an explicit `"target"` field in the normalized schema (defaulting to `"self"`), e.g., `{"target": "self.compute_lease", "op": "...", ...}`.
  - Support symbolic interval anchors (`"anchor": "campaign_start" | "scope_created" | "month_start"`) alongside fixed ISO dates.
  - Store integer-snapped (`16px` grid) node `(x, y)` coordinates in `authoring/layouts/<flow_id>.layout.json`, and keep viewport pan/zoom/selection state in ignored `.work/editor-session.json`.

---

## 4. Missing Gameplay Features & Century-Scale Pacing Enhancements

To ensure the 100-year span (`2014–2114`) remains engaging and avoids mid-project mechanical redesigns during `M4–M8`, the following systemic gaps should be filled.

### 4.1 Connecting the Option Pool (`500 bp`), Talent Scarcity, and Executive Spin-Offs
- **Where in Plan:** `docs/GAME_DESIGN.md` (Section 7, lines 125–130; Section 9, lines 147–151), `docs/SIMULATION_SPEC.md` (Section 3, line 80; Section 6, lines 154–162), `docs/examples/slice-balance.example.json` (line 15).
- **The Gaps:**
  1. **Option Pool has no mechanics:** `slice-balance.example.json` reserves `500` basis points (5%) for `"option_pool"`, and `SIMULATION_SPEC.md` (line 80) lists it on the opening cap table, but `SIMULATION_SPEC.md` never specifies how option pool basis points are spent, vested, or replenished during financing rounds.
  2. **Rival Attrition Over 100 Years:** The default campaign schedules 12 startups (6 domestic, 6 international) and 3 heavyweights (`GAME_DESIGN.md`, line 147). Over a 100-year campaign, if 70% of startups fail or merge in the first 25 years (2014–2039), the remaining 75 years (2040–2114) can become uncompetitive.
- **Recommendations:**
  - **Equity Compensation Policy:** Allow the player (and rivals) to set team and executive compensation policies as **Pure Cash** or **Cash + Equity Grant (drawn from `option_pool`)**. Using equity reduces monthly cash payroll burn for early startups and increases executive retention until vested, while exhausting the `option_pool` requires a Board vote to expand the pool (diluting existing shareholders, including the founder).
  - **Dynamic Spin-Off Rivals from Executive Departures (`M4-01` / `M7-01`):** When a named executive (e.g., Chief Scientist or Head of Safety) resigns from the player's company or a heavyweight after an unresolved dispute (`SIMULATION_SPEC.md`, line 158), allow them—if market capital conditions permit and total active institutions are below the 32-company cap—to **found a new spin-off rival startup** carrying their values profile and public knowledge! This mirrors real AI industry history (e.g., OpenAI, Anthropic, xAI, SSI, Mistral spin-offs) and keeps the late-century competitive ecosystem alive.

### 4.2 Preventing "Year 40–100 Click Fatigue" (Mid-to-Late Game Delegation)
- **Where in Plan:** `docs/GAME_DESIGN.md` (Section 3, lines 42–47), `docs/ROADMAP.md` (line 66).
- **The Issue:**
  - A 100-year campaign spans **36,525 simulated days** (1,200 monthly cycles). While `GAME_DESIGN.md` (line 45) includes delegation for contract renewals, cash reserves, hardware replacement, evaluation scheduling, and price bands, three high-frequency loops remain manual:
    1. Selecting a new research node every 60–120 days across 60 nodes and ongoing incremental improvements.
    2. Updating/retraining existing commercial models when datasets refresh so mature products don't decay in competitive utility.
    3. Dismissing routine informational bulletins during `Fastest` speed.
- **Recommendations:**
  - **Research Queue & Track Allocation:** Allow queueing up to 5 research nodes in sequence on the Research Graph, plus a late-game "Standing Track Policy" for repeatable efficiency research.
  - **Automated Product Maintenance Policy:** Allow delegating an `Auto-Refresh Mature Product` policy (bounded by a % of idle compute and a monthly cash ceiling) that automatically fine-tunes/evaluates existing commercial product lines on newly licensed data while the player focuses on frontier breakthroughs and geopolitics.
  - **3-Tier Notification Routing:** Classify every flow's presentation urgency into:
    1. `Modal Auto-Pause` (Board votes, insolvency workout, Congressional subpoena, severe incident, expiring negotiation).
    2. `Actionable Inbox Item` (New optional deal, non-urgent event with a 30-day response window; pauses only at `deadline - 3 days` if unread).
    3. `Background News Ticker / Monthly Digest` (Routine market shifts, minor rival releases; never interrupts `Fastest` or `Advance to Next Milestone`).

### 4.3 Avoiding the "Mathematically Unwinnable Final Decade" in Sustained Victory Windows
- **Where in Plan:** `docs/GAME_DESIGN.md` (Section 10, lines 175–185), `docs/SIMULATION_SPEC.md` (Section 8, lines 184–190).
- **The Issue:**
  - `SIMULATION_SPEC.md` (line 184) states:
    - **World Monopoly:** Rolling final 60 calendar months (2109–2114).
    - **World Domination:** Qualifying regional authority on **every** monthly check for the final 60 months (2109–2114).
    - **World Utopia:** All 5 global thresholds ($\ge 75\text{–}90$) and all 8 regional minimums ($\ge 70$) on **each of the final 120 monthly checks** (2104–2114).
  - **The UX Trap:** Suppose a player pursuing **World Utopia** reaches `2105-06-01` and a regional hazard or war briefly drops one region's ecological resilience from `70` to `69` for one month. Because fewer than 120 months remain before `2114-01-01`, **the player has already lost the campaign in 2105**, yet the game does not end until `2114-01-01`—forcing them to play 8.5 years of a mathematically dead run!
- **Recommendations:**
  1. **Immediate Window-Breach Alert & Overtime/Grace Mechanics:**
     - If a player is inside the final 10-year (Utopia) or 5-year (Monopoly/Domination) window and a monthly check fails, **immediately auto-pause** with a clear diagnostic showing which threshold broke and why.
     - Allow either:
       - **(A) Rolling Window Victory Qualifying Any Time After Year 50 (e.g., 2064–2114):** Sustaining 120 consecutive months of Utopia (or 60 consecutive months of Monopoly/Domination) completes the objective once the window finishes (just as `ASI Race` can conclude early), with an option to keep playing to 2114; **OR**
       - **(B) Deadline Window with a Bounded Grace/RecoveryAllowance (e.g., 58 of the final 60 months, or 116 of the final 120 months, with mandatory qualification on the final 12 months of 2113):** This prevents a single 1-month stochastic weather hazard in 2106 from invalidating a 100-year Utopia run, and if the player truly falls out of reach of the required months, immediately notifies them with an option to enter Postmortem/Sandbox right away rather than waiting until 2114.

### 4.4 Pre-Deployment Red-Teaming vs. Field Incidents (Clarifying Evaluation Mechanics)
- **Where in Plan:** `docs/GAME_DESIGN.md` (Section 6, lines 121–123), `docs/SIMULATION_SPEC.md` (Section 5, lines 139–144).
- **The Opportunity:**
  - `SIMULATION_SPEC.md` (line 140) separates *true simulation capability*, *measured estimates*, *coverage*, *uncertainty*, and *observed field performance*.
  - To make this crystal-clear and fun in `M3` (where players must explain model evaluation in `M3-04`), explicitly represent **Hidden Edge-Case Flaws / Subgroup Failure Modes** as discoverable tags on a trained model (e.g., `"Hallucinates on scanned tables"`, `"Vulnerable to prompt injection in tool use"`, `"Biased rejection rate on regional format"`).
  - Running held-out evaluations and red-teaming **uncovers** these hidden flaw tags before launch so the player can either:
    1. Filter/augment the dataset and run a short targeted fine-tune,
    2. Attach a matching deployment control (e.g., `"Human review on low-confidence scans"`, costing latency/margin), or
    3. Narrow the contract's promised scope.
  - This turns abstract numbers (`coverage: 6500 bp, confidence: 7000 bp`) into concrete, memorable engineering decisions that non-ML players will immediately grasp in the first 30 minutes (`GAME_DESIGN.md`, lines 63–66).

---

## 5. UI/QML Bridge, Determinism & Platform Packaging Guardrails

### 5.1 Diff-Driven `QAbstractItemModel` Updates (Avoiding 10 Hz QML Table Reset Flicker)
- **Where in Plan:** `docs/TECHNICAL_PLAN.md` (Section 4, lines 75–86), `docs/BACKLOG.md` (`M1-03`, `M1-05`).
- **The Issue:**
  - `TECHNICAL_PLAN.md` specifies that the worker thread publishes immutable `GameState` snapshots to the UI at up to 10 Hz while running.
  - In Qt Quick, if `src/bridge/` exposes lists of contracts, research nodes, datasets, or states as `QVariantList` properties or calls `beginResetModel() / endResetModel()` on a `QAbstractItemModel` each time a snapshot arrives, QML `TableView` and `ListView` will destroy and recreate visible row delegates 10 times per second—losing scroll offset, breaking keyboard focus navigation (`M1-05`), and spiking CPU/GPU frame time.
- **Recommendation:**
  - Design `src/bridge/` models around stable `EntityId` keys. When a new `ImmutableSnapshot` arrives on the main thread, diff each row against the previous snapshot:
    - Emit `dataChanged(idx, idx, changedRoles)` for modified rows (keeping QML delegates, focus, and scroll position intact).
    - Call `beginInsertRows` / `beginRemoveRows` only when entities are actually added or removed.

### 5.2 C++20 Cross-Compiler Determinism Guardrails (Clang vs. GCC vs. MSVC)
- **Where in Plan:** `docs/SIMULATION_SPEC.md` (Section 1, lines 25–27), `docs/BACKLOG.md` (`M1-01`).
- **The Issue:**
  - Even with integer arithmetic and custom PRNG streams, three C++ language behaviors differ across Apple Clang (`macos-arm64`), GCC (`linux-amd64`/`arm64`), and MSVC (`windows-amd64`/`arm64`):
    1. **Function argument evaluation order:** In C++20, `compute_event(rng.next_u32(), rng.next_u32())` evaluates arguments left-to-right on Clang/GCC and right-to-left on MSVC, immediately diverging PRNG streams!
    2. **`std::sort` tie-breaking:** `std::sort` is not guaranteed to order equivalent elements identically across `libc++`, `libstdc++`, and MSVC STL.
    3. **JSON float serialization:** Accidental use of `double` in `nlohmann::json`.
- **Recommendations for `M1-01`:**
  - **PRNG API Design:** Never call `rng.next()` twice as inline function arguments; bind each draw to a named local variable on its own statement line.
  - **Total-Order Comparators:** Every comparator passed to `std::sort` in `src/core/` must end with a tie-breaker on the unique `EntityId` (`std::tie(score_a, a.id) < std::tie(score_b, b.id)`).
  - **CI Hash Parity Gate:** In `M1-01`, add a 1,000-day scripted command sequence test that asserts the exact same 64-bit state hash across the macOS Clang, Linux GCC, and Windows MSVC CI jobs on every PR.

### 5.3 Modern macOS 15+ (Sequoia/Tahoe) Quarantine & itch.io Client Guidance
- **Where in Plan:** `docs/TECHNICAL_PLAN.md` (Section 2, line 27; Section 9, line 151), `docs/QA_AND_RELEASE.md` (Section 8, line 85).
- **The Issue:**
  - The plan correctly specifies ad-hoc signing (`codesign --force --deep -s -`) for macOS arm64 without requiring a paid Apple Developer ID notarization account, and notes that install instructions must be accurate without telling users to disable system-wide security.
  - On macOS 15 (Sequoia) and later, Apple removed the legacy Finder *Control-click $\rightarrow$ Open* bypass for unnotarized apps downloaded via web browsers (which carry the `com.apple.quarantine` extended attribute).
- **Recommendation:**
  - In the `M9-02` installation notes for macOS:
    1. Recommend installing/launching through the **itch.io desktop app** (which handles local extraction without browser quarantine friction), and
    2. For direct browser `.zip` downloads on macOS 15+, document the official per-app Apple flow: attempt to open the app once, then open **System Settings $\rightarrow$ Privacy & Security** and click **Open Anyway** next to the application name.

---

## 6. Prioritized Action Matrix by Milestone

The table below maps each recommendation to the exact milestone and backlog task where it should be incorporated so no existing milestone structure needs to be torn down.

| Priority | Milestone / Task | Recommendation Summary | Prevents |
| :--- | :--- | :--- | :---
| **P0** | **M0-01 / M0-03** | Replace required `Qt6Multimedia` with embedded `miniaudio` + `stb_vorbis` (or make `Qt6Multimedia` optional) and parse `/opt/Qt/6.12.0/macos/sbom/*.json` for license inventory. | Immediate `M0-03` build failure on local macOS and FFmpeg codec bloat on CI. |
| **P0** | **M0-02 / M0-04** | Pin Xcode Clang (`/usr/bin/clang++` + `xcrun --show-sdk-path`) in macOS CMake presets; keep QML compatible with Qt 6.4–6.12 for `linux-arm64` / `windows-arm64` CI flexibility. | macOS `libc++` rpath conflicts and ARM64 CI Qt installer blockers. |
| **P0** | **M0-03** | Add a 25-line `QQmlAbstractUrlInterceptor` + null `QNetworkAccessManagerFactory` in `src/app/` blocking all non-local URLs. | Accidental remote QML/image/font network requests violating `R19`. |
| **P0** | **M1-01** | Add 128-bit intermediate checked math (`mul_div_round_half_up`), strong unit types (`Cents`, `MilliCU`, `BasisPoints`), total-order sort tie-breakers, and Constant-2014-Cent internal accounting. | `int64_t` overflow in utility squaring / 500-year sandboxes and cross-OS RNG divergence. |
| **P1** | **M1-02 / M1-06** | Unify effect `"target"` bindings in schemas, add relative interval anchors (`campaign_start`, `month_start`), and implement a **bounded continuation stack** (`max_depth = 8`) for numbered `Sequence` flows. | Save/load corruption when a `Sequence` branch contains a `Delay` or `Dialogue`. |
| **P1** | **M1-03 / M1-05** | Pause daily ticks at the **Post-Phase-2 Morning Boundary**, allow same-day (`0` elapsed days) refund/cancellation of new orders, and diff snapshots into `EntityId`-keyed `QAbstractItemModel`s. | Missing morning events while paused, punitive misclicks, and 10 Hz QML focus/scroll loss. |
| **P1** | **M2-02 / M2-03** | Define `inference_milli_CU_per_task` on models, add a deterministic 2-constraint water-filling demand algorithm with stateful `installed_task_share` churn limits, and add **Wholesale Compute Leasing** contracts. | Disconnected inference capacity math, erratic daily market-share swings, and an unplayable *Infrastructure Provider* path. |
| **P1** | **E1 / M3** | Split `E1` into **E1a** (Shared runtime + Content Library + Outline/Forms + Before/After Inspector, including a 3-step Synthetic Continuous Preview mode) before `M3`, and **E1b** (2D Node Canvas + Batch Finalizer) alongside `M3`. | 4–7 week delay to first playable (`M3`) and rework of canvas nodes after playtesting. |
| **P2** | **M3-01 / M3-02** | Represent subgroup evaluation gaps as concrete discoverable **Model Flaw Tags** that can be fixed via data curation, targeted fine-tuning, or deployment guardrails. | Abstract/confusing evaluation numbers during `M3-04` novice playtests. |
| **P2** | **M4-01 / M4-03** | Connect the `500 bp` `option_pool` to cash-vs-equity hiring/retention, and allow departed executives to found **spin-off rival startups** when active rivals fall below a floor. | Dead option-pool data field and empty competitor rosters in Years 2045–2114. |
| **P2** | **M7-03 / M8-01** | Add Research Queueing, Auto-Refresh policies for mature products, 3-tier Inbox filtering, and either rolling victory windows or immediate window-breach alerts/grace allowances for 60/120-month objectives. | Late-century click fatigue and playing 9 dead years after a single 1-month Utopia dip in 2105. |
