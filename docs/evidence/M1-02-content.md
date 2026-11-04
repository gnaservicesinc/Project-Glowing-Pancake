# M1-02 foundation content registry and typed validation

Implemented 2026-11-04 against baseline 1.2. The initial supported schema is
[documented beside its owning adapter](../../src/content/README.md). No production
content, release identity, execution engine or editor was created by validation.

`pgp::content` contains a bounded, duplicate-key-aware JSON reader, strong typed
scenario/research/deal/flow definitions, and an extensible fact/effect/system
registry. Unknown operations, fields, target bindings and definition kinds fail
closed. The public API and core have no JSON library or Qt types. The parser
accepts a caller-supplied byte string and has no file/network capabilities.

The original [design examples](../examples/README.md) remain illustrative.
[The engineering fixture](../../tests/content/fixtures/foundation.json) explicitly
normalizes the opening balance and all seven illustrative flows into schema 1:
11 definitions total (one scenario, one research record, two deal references and
seven flows), with separate localization keys and no draft/saved-state notes.
The fixture is excluded from game resources. Loading it does not mean that any
flow has executed or its placeholder research/deal semantics are playable.

Observed locally: the Release content target compiles under Apple Clang with
project warnings treated as errors. The initial direct test run passed **134
checks** in about 0.2 seconds. Subsequent boundary tests are included in the root
integration run; the implementation log records that final result.

Checks cover exact opening balances/ownership/units; all five lifecycle modes;
ordered nested sequence/delay shapes; strict integer tokens and lossless uint64
seed; schema/revision/SemVer and Gregorian boundaries; duplicate keys/IDs;
missing/wrong-kind references; prerequisite, immediate, delayed and subflow
cycles; ownership/ranges; target/scope/unit/argument mismatches; explicit default
choices and reachable nodes; document/depth budgets; and registry extension with
new hidden facts/effects/provider dependencies. Exact eight/nine frame limits
and semantic registry ranges are checked.

Independent review identified an exponential branch-walk hazard in the initial
continuation-depth validator. It was replaced before delivery with reverse-edge
bounded longest-depth propagation. A 12-node graph with eight converging choices
per node (8^12 possible paths) now validates by its nodes/edges rather than path
enumeration. The review also caught an eight-frame off-by-one for subflow chains;
the final validator counts frames, allowing nine flows connected by eight calls.

Dependency: unmodified nlohmann/json **3.12.0**, MIT; exact source URLs, lengths,
SHA-256 and license are preserved under [third_party/json](../../third_party/json/README.md).
Builds never download it. Only the adapter privately includes it.

Remaining boundaries: future catalog schemas extend the typed loader with their
owning systems; unknown kinds are refused today. M1-06 must compile/execute
lifecycles and effects with continuation receipts, budgets and traces. M1-04
implements save IO; E1a implements graphical authoring/finalization and reviewed
content snapshots. Native CI includes this Qt-free test but a local pass does
not imply remote compiler or gameplay/platform qualification.
