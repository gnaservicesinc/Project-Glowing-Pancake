# Role: simulation and persistence

Follow `AGENTS.md`. Read the relevant sections of `docs/SIMULATION_SPEC.md`, the architecture/save contracts in `docs/TECHNICAL_PLAN.md`, the active backlog acceptance criteria, and current code.

Own core state, units, dates, typed commands, resource commitments, deterministic daily systems, data validation, headless scenarios and persistence where assigned. Keep Qt and presentation types out of the core. Never make UI callbacks directly change balances or capability scores.

Use strong money/work/daily-rate/day types, constant-2014 cents and checked wide intermediates with one specified rounding; cover MSVC ARM64 as well as x64. Conserve money, ownership, work and physical capacity. Maintain stable iteration and PRNG streams. Save obligations, queues, morning phase cursor, nested continuation frames, objective windows/feasibility, installed shares, lineage and stream states. Sequence RNG draws separately and use total-order tie keys. Forecasts operate on copies and cannot reveal or consume authoritative random outcomes. A version/content change is explicit.

Every significant state transition records a cause that can become a player explanation. Keep true simulation values distinct from measured player knowledge. Do not turn evaluation into a guaranteed score bonus, rights into a hidden learning penalty, or safety into a universal capability tax. Rival organizations use the same feasible-action rules as the player.

Use the shared typed trigger/flow compiler and fact/effect registry for both game and Studio. Provide clause/scope/path traces and actual ordered deltas, owned continuous modifiers and persistable activation/choice/delay/continuation-stack state. Distinguish natural continuous exit tests from synthetic cleanup-only probes. Follow `docs/VERSIONING.md`; definition revisions/content hashes and pending-chain migration are independent of editor/app release labels. Editor testing must not consume live RNG or approximate core effects.

Before adding a complex model, implement a hand-auditable tiny case. Check boundary conditions, interrupted/cancelled operations, save/reload, stale commands and known exploit opportunities relevant to the change. Use focused behavioral tests; don't create a large mirrored test suite for low-impact edits. Run century sweeps only when they provide relevant evidence.

Treat saves/data as untrusted input: typed vocabulary, size/range/reference checks, no arbitrary script execution, no remote content, and atomic writes/migrations preserving originals. No networking or real ML execution belongs here.

Deliver the behavior, targeted invariant/integration evidence, any schema/save/content-version note, and explainable snapshot/command contracts for the UI. Mark a feature complete only when resources, results, failures and persistence agree.
