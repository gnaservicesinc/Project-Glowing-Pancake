# Version tracking and release identity

Planning baseline 1.2 · Updated 2026-11-05 · Shared identity, internal snapshot writer and E1B-02 graphical finalization implemented; release gates remain open

## 1. One source, several different identities

[`project-versions.json`](../project-versions.json) is the checked-in source of game, editor, content, and compatibility versions. Game and editor remain `0.0.0-dev`. The current reviewed internal content snapshot is `0.8.0-dev.1`. The planning phase distinguishes these development identities from publishable artifacts. Compatibility values of 1 identify the initial content, runtime and save contracts. Studio supports reviewed save copies between finalized generations under those contracts. Documentation baseline 1.2 identifies the plan.

| Identity | Meaning | Changes when |
| --- | --- | --- |
| Game version | Version of a distributed game package, including its bundled content | Any released game behavior, code, bundled content, assets, or packaging changes |
| Editor version | Version of the separately distributed Content Studio | Editor code/UI, shared semantics it ships, or its package changes |
| Content version | Version of the reviewed runtime content collection | A finalized batch changes playable definitions, text, localization, assets, or their semantics |
| Definition revision | Positive integer on one flow/event/research/etc. definition | That definition changes in a finalized batch; once per batch, not per keystroke |
| Content schema | Integer defining the content data/graph representation | Loader/compiler representation or required interpretation changes incompatibly |
| Simulation rules | Integer identifying authoritative mechanics | Core rules change outcomes; presentation-only changes do not increment it |
| Save format | Integer defining serialized campaign structure | State representation needs a migration |
| Git commit + content hash | Exact source and runtime-data identity | Source/content changes, independently of a human-readable version |

Use Semantic Versioning for the three component versions, with a declared compatibility contract: patch for fixes/cosmetic corrections, minor for additive features/content, major for breaking the component's supported contract. Before 1.0, a breaking change can increment minor, but compatibility integers/migration requirements still apply explicitly. Do not infer save compatibility from SemVer alone. Prereleases use a supported suffix such as `0.2.0-dev.1` or `0.2.0-rc.1`. [Semantic Versioning specification](https://semver.org/).

A human-readable release version identifies one immutable published payload. Never upload changed game content under an existing game version. Content-only game updates still need a new game package version; changing only the content number would not identify the new distributable. Editing content does not automatically change the editor binary's version.

## 2. Authoring workflow and version controls

**Save to project** validates the edited collection and replaces the reviewed
runtime source in the selected checkout. A semantic change creates an internal
content revision with the same journaled writer used by the maintainer CLI.
Git history, tags and public releases remain separate. The explicit batch
review also accepts separately prepared sources:

1. Gather the selected content change set and compare with the last finalized content snapshot. Include referenced text/assets and show the actual file/semantic diff.
2. Validate the collection, references, trigger/effect graphs, lifecycle rules, units, and required local test fixtures. Invalid drafts may be saved but cannot enter the reviewed runtime collection.
3. Show current/proposed content version, revision increments, a short author-written change summary, affected scenario/definition IDs, compatibility implications, and the proposed minimum game-version change. Recommend a bump; the author chooses with an explanation when necessary.
4. Increment changed definition revisions once, update the content version and content changelog, and update the game version if the author chooses to prepare that next game package now. Otherwise record an unresolved release requirement that CI blocks before packaging a release.
5. Write the batch through staged files, a recovery journal/backups and a commit marker. Recheck source hashes before replacement; do not overwrite external edits or leave versions describing a half-written collection.
6. Show a concise completion summary and the remaining release requirements. Normal repository review/commit/push follows outside this operation; nothing is published.

The author can finalize several changes together. Autosave and canvas moves do not spam version history. A no-op save, pure formatting change, graph layout, bookmarks or local editor preferences do not bump runtime content or definition revisions. A disabled definition remains reviewed content whose enabling could affect play; changes to it are versioned. Draft-only edits remain outside the runtime version until promoted.

The future root `CHANGELOG.md` summarizes game releases. The implemented `authoring/content-history.json` records finalized internal batches, versions, changed IDs/revisions, compatibility declarations, validation and summaries. `authoring/content-state.json` links the immutable baseline, history and runtime manifest by hashes; it is replaced last. Editor release entries remain separate future outputs. See [snapshot commands and recovery](../src/studio/SNAPSHOTS.md) and [planning history](PLAN_CHANGELOG.md).

No tool can reliably infer intent or compatibility from a diff alone. Registry/schema differences, changed references, and fixture results support the author/reviewer. A content major bump never makes a missing save migration acceptable by itself.

## 3. Hashes, build metadata, and release readiness

Finalization generates `data/runtime/content-manifest.json`. The internal slice record contains content/compatibility versions, reviewed definitions/revisions and semantic hashes, scenario flow roots, message hash, source hash and complete collection hash. It includes disabled reviewed definitions. The current slice has no external asset references, so its asset list is empty; broader asset provenance/hashing arrives with E2. Canonical bytes come from the shared typed normalizer; [the writer contract](../src/studio/SNAPSHOTS.md) documents included semantics and separate authoring metadata. Visible text, effect order, timing and target bindings affect identity. A build reads these artifacts without rewriting versions.

The editor graph is the editable logical source. A shared compiler produces a bounded normalized runtime representation for both preview and game packaging. Layout/comments stay in authoring files. The collection hash represents shipped semantics/assets; a source hash can additionally identify the full authoring document. A cosmetic shipped text correction changes content identity even if it does not affect simulation state hashes.

Build metadata includes product, full component version, content/schema/rules/save versions, Git commit, runtime collection hash, target OS/architecture and dependency/source manifest. Generate it from the same metadata for CMake, About/diagnostics, filenames, source bundles, itch uploads and later Snap. CMake receives the numeric SemVer core where required, with prerelease stored separately; do not feed `0.2.0-rc.1` to a numeric-only project-version field.

CI release checks refuse planning placeholders, invalid versions, unresolved content→game bump requirements, dirty/untracked runtime source, tag/manifest mismatch, missing migrations, and reuse of a published version with different content. A reproducible build does not rewrite checked-in version files. Developer builds may show local modifications in diagnostics; publishable builds use reviewed immutable commits. Monotonicity is checked using the relevant component's released history, not by lexicographic string comparison.

Future tag conventions are `game-vX.Y.Z` and `editor-vX.Y.Z`, allowing independently reviewed downloads. A content revision is normally an ordinary repository commit with a content-history entry; it need not publish its own binary or tag. The editor release records the exact game-core/content-schema versions its previews support. Testing with a mismatched validator/runtime is blocked or clearly read-only.

## 4. Existing saves and event chains

Saves retain exact content/rules/schema identity, definition revisions for pending/active flows, node/continuation-stack/context/choice state, daily phase cursor, cooldowns and occurrence counters. A new content version cannot silently give a pending choice different effects, reset a once-only event, or apply a continuous modifier twice.

After a game/platform update, migrations either retain required retired definitions until active instances finish or explicitly map old nodes/IDs/state into compatible definitions. Any changed continuation is documented in migration metadata. Preserve the original save and offer a clear unsupported-version error when no safe path exists. A pure text correction can be compatible, but it still changes shipped content identity. Cross-version continuation is not claimed to reproduce an older ruleset's future exactly.

Release fixtures cover saves paused on each important choice/result, scheduled consequence, cooldown boundary and continuous modifier. This matters because distribution clients may install updates independently of the game. No in-game updater, server, or remote migration lookup is added.

E2-03 implements offline review of an individual campaign against two verified
finalized generations. The separate Studio exports a new save and its review
receipt after validating equivalent saved flow behavior and exact checkpoint
reload. Normal game loads keep exact identity checks. [Migration workflow](../src/studio/SNAPSHOTS.md#compare-finalized-revisions-and-review-a-campaign-copy).

## 5. Future Snapcraft connection

The **game Snap's version is the game package version**. Content version remains embedded metadata; the editor version is not substituted for it. At M10, a small checked-in version reader supplies the game version from `project-versions.json` to a Snapcraft part using `adopt-info` and `craftctl set version=...`. Canonical documents this metadata path. [Snapcraft project reference](https://ubuntu.com/docs/snapcraft/latest/reference/snapcraft-yaml/), [lifecycle metadata](https://ubuntu.com/docs/snapcraft/latest/how-to/crafting/override-the-parts-lifecycle/).

Do not maintain a second manually edited version literal in `snapcraft.yaml`, derive the version from the newest unrelated editor tag, or rename a game update after the editor that created its content. Validate platform-supported version syntax/length when implementing M10; do not silently truncate or replace an invalid value.

Snap Store revisions identify uploaded builds separately from our developer-assigned version. Channels select distributed revisions; our SemVer alone does not control rollout or prove publication. Record architecture/revision/channel alongside the source, version and content hash, and verify refreshed saves. [Snap revision documentation](https://snapcraft.io/docs/explanation/how-snaps-work/revisions/).

The workflow builds the game-only package from a fixed authorized release commit, validates the same content manifest as itch, attaches matching source/licenses/credits, and publishes only under the explicit release task. No editor binary or authoring cache enters the base game Snap. A future editor Snap would be a separately scoped product, not part of M10 by default.

## 6. Implemented identity reader and remaining gates

M0-05 adds [`tools/project_versions.py`](../tools/project_versions.py), a Python
3.10+ standard-library build tool shared by CMake and future packaging consumers.
It accepts only manifest format 1 with the exact declared fields, project ID,
three valid SemVer strings and three positive compatibility integers. Duplicate
JSON keys, unknown/missing fields, booleans/floating-point compatibility values,
unknown phases and oversized/malformed data are rejected. Numeric version cores
and compatibility values are bounded to 2,147,483,647 for the CMake/C++ metadata
contract. SemVer comparison itself supports larger numeric identifiers within
the tool's 256-character version limit.

Accepted phases are `planning`, `development`, `release-candidate`, and `release`.
The checked-in manifest remains `planning`, with game/editor `0.0.0-dev` and
internal content `0.2.0-dev.1`; there is no published product/content release.
The snapshot records an unresolved game requirement of `0.1.0-dev.1`.
Changing phase is an explicit future source
edit. A release-phase product cannot retain a prerelease suffix. Game/editor tags
are independently derived, and a previous-version check uses the requested
product's released history. Build metadata does not affect SemVer precedence and
cannot make a reused version newer.

[`cmake/VersionIdentity.cmake`](../cmake/VersionIdentity.cmake) runs validation
before `project()`, supplies only the game's numeric core to CMake's version
field, and preserves full versions in generated immutable
[`BuildIdentity`](../src/core/include/pgp/core/build_identity.hpp). Core/headless
have no Qt, Python-runtime or network dependency. The shared diagnostics also
report compatibility integers, target OS/architecture, raw manifest SHA-256,
configure-time Git commit/state and independent prospective game/editor artifact
stems. A stem is a naming input, not an existing package. Product binary names
remain unchanged. E1A-04 supplies the finalized runtime collection hash after
the metadata reader verifies the record's linked files, source bytes and versions.
Absent/invalid records report it as `unavailable`; the version-file hash is never
substituted. The bundled game independently compiles its actual resource and
refuses a mismatch with an available configured hash.

Source identity is a snapshot at configuration, clearly labeled in diagnostics.
The wrapper configures on each invocation; CMake also watches the version and
snapshot/runtime files. Direct `cmake --build` alone need not notice an unrelated
source edit/commit, so reconfigure before relying on that observation.
Missing Git/exported trees report unavailable identity. See
[build commands](BUILDING.md) for the CLI and explicit reconfigure instructions.

The `check-release` command checks phase/placeholders, exact product tag,
optional previous-version precedence and live Git cleanliness. It separately
reports the identity gate, snapshot record integrity and whether the actual game
version meets the recorded content requirement. The Python record check does
not replace the C++ compiler/runtime validation. The command **always returns nonzero with
`release_ready: false`** until the following owning gates exist:

- E2 and M9: reviewed compatibility evidence for the specific released version
  pairs. E2-03 provides the offline comparison, migration and verification path
  for internal generations.
- M9: released-history/payload-reuse enforcement, dependency/source/license
  manifest, native package and clean-device evidence.

M10 later consumes the existing identity for game-only Snap metadata and
qualified refresh behavior; it does not block the primary release's readiness.

These checks cannot be supplied as a handful of command-line approval booleans.
The metadata reader does not finalize content or rewrite manifests. The separate
`pgp-content-tool` and the Studio graphical finalizer implement internal
finalization through the journaled shared writer, record required game bumps
and preserve draft bytes. Cross-content save review is an explicit Studio
operation separate from finalization.
Future gate implementations must use actual owning artifacts and maintain this
fail-closed behavior. A pending snapshot transaction blocks configuration and
must be recovered before building.

## 7. Implementation sequence and acceptance

- M0-05 implements the metadata reader/checks and shared build identity described above. The project remains in planning phase; later release gates remain open.
- M1-02/M1-06 implement schema/revision/hash/event-state contracts. E1A-04 supplies the safe validation/version tool and first reviewed internal snapshot. E1B-02 implements graphical batch finalization/version controls through that same writer, completing the Mac component prerequisites for M4. Neither stage bypasses revisions, conflict checks, recovery or package identity. Human author qualification waits for v1 preparation after all components are in place, per the 2026-11-05 user instruction.
- E2 adds impact/compatibility tools for broader content and tests save continuation across content revisions.
- M9 connects tag/version/artifact/source/itch identity; M10 connects the existing reader to Snapcraft rather than inventing new version numbers.

Acceptance examples: one text fix produces one definition revision and a patch content bump; a canvas rearrangement produces neither; a batch touching three definitions increments each once; a finalized content-only game update requires a new game package version; editor-only UI work does not bump content; a stale external edit cannot be overwritten; interrupted finalization recovers consistent versions/files; a pending dialogue migrates without changing an already committed choice; game/editor tags cannot be mixed in the future Snap pipeline.
