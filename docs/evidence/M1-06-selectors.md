# M1-06 related-entity predicates

The shared schema-1 compiler and runtime now support typed `related` conditions
with `any`, `all` and integer `count` comparisons. The registry exposes the
company's compute sources, projects and contracts, and each compute source's
own projects. The reader resolves IDs from the authoritative capacity and market
rows. It does not create an independent copy of business facts. See the
[authoring schema](../../src/content/README.md) and
[compiler contract](../../src/runtime/COMPILER.md).

The inspector validates member type and identity, sorts by stable ID, and
records selected members, per-member truth and match-count bounds. Hidden
relations omit membership altogether in player mode. An unknown member's
truth widens the count interval, so only a publicly decisive comparison can
return a definite result. Existing authored order is retained around selectors.
Nested selectors and every member count toward a 16,384-node inspection limit.
Daily sampling also shares the existing duration operation budget.

The live fixture starts with a serving contract and two compute sources, one
empty. A command reserves one unit of real training work on the other source.
The aggregate timer starts after that work commits. An exact save at zero
elapsed days resumes at the same threshold as the uninterrupted campaign, with
one decision receipt and unchanged random streams. The test also checks
canonical recompile, empty collections, hidden membership/facts, count
thresholds, invalid providers, invalid schema payloads and the expanded budget.

Apple arm64 optimized desktop build passed. The selector executable passed
32 assertions, and all 11 focused CTests passed, covering content, compiler,
runtime, duration, variables, saves, capacity, market and liquidity.

No save-format or existing content-version change was required. New content
using a selector includes its relation and comparison in its canonical hash.
The timer belongs to the enclosing flow scope. Per-member timer state, root
flows on business member scopes and multi-entity effects are outside this
batch. M1-06 and M1-04 still retain their prerequisite gates. Next Mac task is
M1-07 committed and seeded event dispatch.
