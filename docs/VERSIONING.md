# Version tracking and release identity

Planning baseline 1.1 · 2026-11-03 · Metadata initialized; automation is future implementation

## 1. One source, several different identities

[`project-versions.json`](../project-versions.json) is the checked-in source of game, editor, content, and compatibility versions. The initial `0.0.0-dev` values mean no application/content release exists. `phase: planning` prevents those placeholders being mistaken for publishable artifacts. Compatibility values of 1 reserve the first formats; no corresponding loader/schema has been implemented yet. Documentation baseline 1.1 is not application version 1.1.

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

Ordinary saves preserve drafts/work in progress and mark pending content changes. They do not create a public version, tag, commit, push, or store upload. The Studio has a visible **Finalize content revision** action:

1. Gather the selected content change set and compare with the last finalized content snapshot. Include referenced text/assets and show the actual file/semantic diff.
2. Validate the collection, references, trigger/effect graphs, lifecycle rules, units, and required local test fixtures. Invalid drafts may be saved but cannot enter the reviewed runtime collection.
3. Show current/proposed content version, revision increments, a short author-written change summary, affected scenario/definition IDs, compatibility implications, and the proposed minimum game-version change. Recommend a bump; the author chooses with an explanation when necessary.
4. Increment changed definition revisions once, update the content version and content changelog, and update the game version if the author chooses to prepare that next game package now. Otherwise record an unresolved release requirement that CI blocks before packaging a release.
5. Write the batch through staged files, a recovery journal/backups and a commit marker. Recheck source hashes before replacement; do not overwrite external edits or leave versions describing a half-written collection.
6. Show a concise completion summary and the remaining release requirements. Normal repository review/commit/push follows outside this operation; nothing is published.

The author can finalize several changes together. Autosave and canvas moves do not spam version history. A no-op save, pure formatting change, graph layout, bookmarks or local editor preferences do not bump runtime content or definition revisions. A disabled definition remains reviewed content whose enabling could affect play; changes to it are versioned. Draft-only edits remain outside the runtime version until promoted.

The future root `CHANGELOG.md` summarizes game releases; `authoring/content-history.json` records finalized content batches, versions, changed IDs/revisions, compatibility declarations and summaries. Editor release entries are separate. These files are planned outputs, not present implementation. The planning history is [PLAN_CHANGELOG.md](PLAN_CHANGELOG.md).

No tool can reliably infer intent or compatibility from a diff alone. Registry/schema differences, changed references, and fixture results support the author/reviewer. A content major bump never makes a missing save migration acceptable by itself.

## 3. Hashes, build metadata, and release readiness

Generate a runtime content manifest during validation/build. It records content version, schema/rules requirements, enabled definitions and revisions, normalized definition hashes, localization and asset hashes, and the complete collection hash. Canonicalization uses a documented encoding, key ordering, normalized paths and explicitly classified semantic fields. Exclude only known authoring-only fields; never accidentally exclude visible text, effect order, timing, target bindings or asset changes.

The editor graph is the editable logical source. A shared compiler produces a bounded normalized runtime representation for both preview and game packaging. Layout/comments stay in authoring files. The collection hash represents shipped semantics/assets; a source hash can additionally identify the full authoring document. A cosmetic shipped text correction changes content identity even if it does not affect simulation state hashes.

Build metadata includes product, full component version, content/schema/rules/save versions, Git commit, runtime collection hash, target OS/architecture and dependency/source manifest. Generate it from the same metadata for CMake, About/diagnostics, filenames, source bundles, itch uploads and later Snap. CMake receives the numeric SemVer core where required, with prerelease stored separately; do not feed `0.2.0-rc.1` to a numeric-only project-version field.

CI release checks refuse planning placeholders, invalid versions, unresolved content→game bump requirements, dirty/untracked runtime source, tag/manifest mismatch, missing migrations, and reuse of a published version with different content. A reproducible build does not rewrite checked-in version files. Developer builds may show local modifications in diagnostics; publishable builds use reviewed immutable commits. Monotonicity is checked using the relevant component's released history, not by lexicographic string comparison.

Future tag conventions are `game-vX.Y.Z` and `editor-vX.Y.Z`, allowing independently reviewed downloads. A content revision is normally an ordinary repository commit with a content-history entry; it need not publish its own binary or tag. The editor release records the exact game-core/content-schema versions its previews support. Testing with a mismatched validator/runtime is blocked or clearly read-only.

## 4. Existing saves and event chains

Saves retain exact content/rules/schema identity, definition revisions for pending/active flows, node/context/choice state, cooldowns and occurrence counters. A new content version cannot silently give a pending choice different effects, reset a once-only event, or apply a continuous modifier twice.

After a game/platform update, migrations either retain required retired definitions until active instances finish or explicitly map old nodes/IDs/state into compatible definitions. Any changed continuation is documented in migration metadata. Preserve the original save and offer a clear unsupported-version error when no safe path exists. A pure text correction can be compatible, but it still changes shipped content identity. Cross-version continuation is not claimed to reproduce an older ruleset's future exactly.

Release fixtures cover saves paused on each important choice/result, scheduled consequence, cooldown boundary and continuous modifier. This matters because distribution clients may install updates independently of the game. No in-game updater, server, or remote migration lookup is added.

## 5. Future Snapcraft connection

The **game Snap's version is the game package version**. Content version remains embedded metadata; the editor version is not substituted for it. At M10, a small checked-in version reader supplies the game version from `project-versions.json` to a Snapcraft part using `adopt-info` and `craftctl set version=...`. Canonical documents this metadata path. [Snapcraft project reference](https://ubuntu.com/docs/snapcraft/latest/reference/snapcraft-yaml/), [lifecycle metadata](https://ubuntu.com/docs/snapcraft/latest/how-to/crafting/override-the-parts-lifecycle/).

Do not maintain a second manually edited version literal in `snapcraft.yaml`, derive the version from the newest unrelated editor tag, or rename a game update after the editor that created its content. Validate platform-supported version syntax/length when implementing M10; do not silently truncate or replace an invalid value.

Snap Store revisions identify uploaded builds separately from our developer-assigned version. Channels select distributed revisions; our SemVer alone does not control rollout or prove publication. Record architecture/revision/channel alongside the source, version and content hash, and verify refreshed saves. [Snap revision documentation](https://snapcraft.io/docs/explanation/how-snaps-work/revisions/).

The workflow builds the game-only package from a fixed authorized release commit, validates the same content manifest as itch, attaches matching source/licenses/credits, and publishes only under the explicit release task. No editor binary or authoring cache enters the base game Snap. A future editor Snap would be a separately scoped product, not part of M10 by default.

## 6. Implementation sequence and acceptance

- M0-05 implements the metadata reader/checks and shared build identity. `project-versions.json` currently supplies only the initial tracking record.
- M1-02/M1-06 implement schema/revision/hash/event-state contracts; E1 supplies graphical batch finalization and version controls.
- E2 adds impact/compatibility tools for broader content and tests save continuation across content revisions.
- M9 connects tag/version/artifact/source/itch identity; M10 connects the existing reader to Snapcraft rather than inventing new version numbers.

Acceptance examples: one text fix produces one definition revision and a patch content bump; a canvas rearrangement produces neither; a batch touching three definitions increments each once; a finalized content-only game update requires a new game package version; editor-only UI work does not bump content; a stale external edit cannot be overwritten; interrupted finalization recovers consistent versions/files; a pending dialogue migrates without changing an already committed choice; game/editor tags cannot be mixed in the future Snap pipeline.
