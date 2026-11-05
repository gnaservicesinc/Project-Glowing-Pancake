# Reviewed internal content snapshots

E1A-04 implements a Qt-free writer shared by the maintainer CLI and the future
E1B-02 graphical finalizer. It uses the same collection compiler, runtime,
scenario creation and exact save validator as the game. Draft editing remains
in Content Studio's forms; this tool reviews and promotes a complete collection.

The first internal snapshot is content **0.1.0-dev.1**, generation 1, with
collection SHA-256
`a09060be69908bea63cd9b6fa04e06f275f1226cfaf6977a7e0d7ec647b0c26f`.
Game/editor remain **0.0.0-dev** and the project remains in planning phase.
The required game version **0.1.0-dev.1** is unresolved. No product release,
human test session or publication is implied by this internal content record.

## Explicit maintainer workflow

Build the opt-in editor configuration once, then retain the optimized tree:

```sh
./make.sh --editor --jobs 4
build/release/bin/pgp-content-tool check --checkout "$PWD"
```

Edit a current-version collection through Studio and save it to a separate
`data/drafts/` file. Review a later batch with a new local plan filename:

```sh
build/release/bin/pgp-content-tool review --checkout "$PWD" \
  --source data/drafts/reviewed-slice.json \
  --version 0.1.1-dev.1 --summary 'Correct the reviewed supplier message.' \
  --plan .work/reviewed-slice-plan.json
build/release/bin/pgp-content-tool finalize --checkout "$PWD" \
  --plan .work/reviewed-slice-plan.json
build/release/bin/pgp-content-tool check --checkout "$PWD"
```

Read the report before running finalize. It includes definition before/after
values, revision increments, text keys, observed hashes, proposed writes,
version requirements, compiler lints and scenario validation results.
Review writes only the explicitly selected new `.work/*.json` plan; omitting
`--plan` prints the report without writing. Finalize rebuilds the full plan and
refuses any mismatch. Editing plan text cannot add arbitrary destinations or
bypass validation. A source may also be a reviewed `data/runtime/` collection.

An optional `--game-version VERSION` on review explicitly proposes a newer game
version satisfying outstanding content requirements. Leaving it out records
the unresolved requirement. Neither option changes the project phase or editor
version. The CLI never commits, tags, pushes, uploads or publishes. Repository
review and Git preservation remain separate actions.

## Version and validation rules

- One slice collection per checkout, with an existing `data/runtime/*.json`
  target. The default is `data/runtime/first-playable.json`.
- Source content version and existing definition revisions must match the last
  finalized baseline. Reopen/rebase stale drafts through the supported authoring
  path. Do not manually pre-increment revisions or content metadata.
- Changed definitions increment once per batch; new definitions start at 1.
  Referenced visible messages count toward their definition's signature.
  Scoped variable changes conservatively affect all definitions. Removed IDs
  are recorded. Disabled definitions remain reviewed, versioned content.
- A runtime change needs an explicit short summary and a strictly newer bounded
  SemVer content version. Definition-body/addition/removal changes require at
  least a minor game bump; message-only changes require a patch game bump.
  Outstanding requirements carry forward until an explicit game version meets
  them. This classification does not infer save compatibility or author intent.
- Formatting and semantic no-ops do not increment content, definitions or the
  content-history generation. Any actual formatting/version write still gets
  a distinct transaction marker sequence. Layouts and local editor state live
  outside runtime collections and do not enter their hash.
- Every scenario/headquarters opening executes through the real prepared
  morning and exact save roundtrip. Limits are 16 scenarios, 64 openings,
  128 history batches and bounded JSON bytes/depth/tokens. The current slice
  validates 15 definitions, one scenario and three headquarters. This proves
  opening/save consistency; full scenario and semantic review remain separate.
- Save policy is explicitly `reject-prior-content`: the real save loader rejects
  mismatched content identity and preserves the original save. This tool cannot
  claim a migration. Cross-revision continuation review remains E2-03.

## Owned artifacts and hashes

| Path | Purpose |
| --- | --- |
| `data/runtime/first-playable.json` | Normalized reviewed runtime collection |
| `data/runtime/content-manifest.json` | Content/compatibility versions, collection/source hashes, definition signatures/revisions, message hash and scenario flow roots |
| `authoring/snapshots/NNNNNN.collection.json` | Immutable baseline for that generation |
| `authoring/content-history.json` | Batch summary, changed definitions/text, versions, validation and save policy |
| `project-versions.json` | Independent component and compatibility versions |
| `authoring/content-state.json` | Final commit marker linking artifact hashes and outstanding game requirement |
| `.pgp-editor/snapshot-transaction/` | Temporary local recovery journal/staged bytes; excluded from packages and Git |

Canonical collection bytes come from `content::collection_json` through the shared
compiler: bounded UTF-8 JSON with ordered object keys, normalized typed values
and stable definition/catalog ordering. Effect/child/choice order, timing,
bindings, visible messages and version/revision fields remain semantic. The
SHA-256 collection identity is the same value used by actual saves. Pretty
source bytes have their own hash. Per-definition signatures omit only the
revision itself and include referenced messages and scoped variables. The
current slice has no external asset references; its manifest asset list is
empty. Broader asset/catalog authoring remains E2 work.

Build metadata reads the committed record and exposes its hash after verifying
its artifact links. The bundled game also compiles its resource and verifies
the actual collection hash against that build identity. Explicit developer
`--content` fixtures remain separately selected. CMake watches snapshot records
and runtime source so finalization refreshes the embedded identity. The normal
game resources contain only the runtime collection/manifest, with no drafts,
immutable authoring baselines, journal, editor help/QML or editor executable.

## Recovery and conflicts

Finalization exclusively creates the local transaction directory, flushes a
complete checksummed journal and stages before/after bytes before changing any
tracked target. It rechecks observed files and each destination immediately
before atomic replacement. The content-state marker is replaced last; a file
system does not make the whole multi-file batch atomic.

If interrupted, stop editing the affected files and run:

```sh
build/release/bin/pgp-content-tool recover --checkout "$PWD"
build/release/bin/pgp-content-tool check --checkout "$PWD"
```

An incomplete batch restores the prior complete generation. A committed marker
causes all after-bytes to be verified before clearing the completed journal.
Recovery first checks every destination; a concurrent third version causes
refusal without overwriting it. Preserve that edit, reconcile it explicitly,
then retry recovery and review. Unsupported paths, links, unexpected journal
files or incomplete/corrupt journal evidence are preserved for inspection.
Draft saves, snapshot reviews and normal build configuration refuse a pending
transaction. Draft source bytes are never part of the promoted write set.

`tools/project_versions.py check-release` reports record integrity and the
outstanding game requirement separately from its identity gate. Release readiness
stays false while migration/released-history/dependency/native-package gates
remain unimplemented or unqualified. Passing `check` is internal snapshot
validation, not a release approval.
