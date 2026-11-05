# Reviewed internal content snapshots

E1A-04 implements a Qt-free writer shared by the maintainer CLI and the
E1B-02 graphical finalizer. It uses the same collection compiler, runtime,
scenario creation and exact save validator as the game. Draft editing remains
in Content Studio's forms; this tool reviews and promotes a complete collection.

The first internal snapshot is content **0.1.0-dev.1**, generation 1, with
collection SHA-256
`a09060be69908bea63cd9b6fa04e06f275f1226cfaf6977a7e0d7ec647b0c26f`.
Game/editor remain **0.0.0-dev** and the project remains in planning phase.
The required game version **0.1.0-dev.1** is unresolved. No product release,
human test session or publication is implied by this internal content record.

The M4-01 snapshot is **0.2.0-dev.1**, generation 2, hash
`04e2761cd39c7adb788e754562f3ecb4fa072ee564a89ac006febcbdb7f72668`.
M4-01 introduces four competition presets at revision 1, leaving the original
15 definitions unchanged. All five scenarios / 15 headquarters openings pass
the real prepared-save roundtrip. Game/editor and the unresolved required game
version remain as above; older content saves are refused without a migration.

The M4-02 snapshot is **0.3.0-dev.1**, generation 3, hash
`a55565fbf1a22b59b8166a4729f99823b0df918e581ee0d52dc0a238a9bf7563`.
Four full-roster presets add heavyweight response policies and public financing
disclosures. The shared competitive-notice cursor now allows 4,096 notices,
matching its saved history bound; that root variable participates in every
definition signature, so the writer correctly advances all 19 definitions to
revision 2. All five scenarios / 15 prepared headquarters saves round-trip with
no lints. Generations 1 and 2 remain immutable. Game/editor stay 0.0.0-dev with
the unresolved game requirement above; this is internal development content.

The current M4-03 snapshot is **0.4.0-dev.1**, generation 4, hash
`479c32eb0b657602357f087450b0d1ead27cb6cf50837ec7651c11fba921915e`.
The four full-roster scenarios add negotiated financing, board rules, executive
compensation/promises and conserved acquisition/group-transfer terms; those four
advance from revision 2 to 3. The other 15 definitions remain revision 2. All
five scenarios / 15 prepared headquarters saves round-trip without lints.
Game/editor and unresolved release requirements remain as above.

## Graphical author workflow

Open the separate Content Studio on the chosen checkout. Edit a collection
through its canvas/forms and **Save as draft** (or **Save draft**). Select
**Review content revision** in the top toolbar:

1. Choose a saved collection from the dropdown; the working collection is
   selected initially. Engineering fixtures are excluded. The current bounded
   writer promotes into `data/runtime/first-playable.json`.
2. Enter a newer content version and a short change summary. A minor development
   version is suggested; an author can choose a patch for a wording fix. An
   optional explicit game-version proposal must meet all pending requirements.
3. Select **Review saved batch**. Read the component versions, scenario/opening
   and exact-save checks, compiler notices, changed definitions/revisions,
   before/after field values and visible text, and proposed file hashes.
   Choose each changed definition in the dropdown to inspect its fields.
   Values use exact integer text, including values beyond JavaScript's safe
   integer range. All differences are read-only.
4. Acknowledge the displayed differences and select **Finalize reviewed batch**.
   The same shared writer rebuilds and verifies the reviewed plan before any
   replacement. The completion summary records the internal content identity
   and unresolved game/release requirements. A clean working collection reopens
   from the finalized runtime with its new revisions; the original draft stays
   separate and is preserved.

No command or text editor is needed for these author actions. An obsolete draft
cannot overwrite the new baseline; continue from the reopened runtime and save
a new draft. Changing source/version/summary or the working content clears the
review. Unsaved content must be saved before review. Layout moves remain outside
the plan; save or undo them before a batch that writes files so the working
layout can reopen safely. A semantic no-op with no file writes can be verified
without discarding unsaved layout moves. Viewport gestures do not invalidate a
review or change runtime identity.

An external edit causes refusal and consumes the stale review. Review again
only after inspecting the current saved source. If interrupted, reopen the
dialog and select **Recover interrupted content batch**. Recovery uses the
same journal checks and preserves a concurrent third version; a refusal leaves
the journal and the conflicting bytes intact. Unsaved working content is not
discarded by recovery. Corrupt/unknown evidence still requires explicit
repository reconciliation; no overwrite or compatibility guess is offered.

The dialog uses ordinary keyboard-accessible controls, scrolling differences
and fixed review/finalize buttons. Its engineering path is explicit:

```sh
git worktree add --detach .work/e1b02-demo HEAD
build/release/bin/pgp-editor --project "$PWD/.work/e1b02-demo" \
  --finalizer-walkthrough "$PWD/.work/e1b02-demo-report.json" \
  --screenshot "$PWD/.work/e1b02-demo.png"
```

Use new copy/report/image names. This runs the forms and canvas walkthroughs,
then the actual QML review/finalize/reopen controls, a preserved external edit,
no-op verification, deliberate partial-write recovery and explicit game-version
resolution **only in the selected copy**. The PNG captures the displayed batch
differences before finalization. The current Mac Metal run reviews six changed
definitions as one generation-2 batch, preserves the source drafts and retains
release readiness as false. It records **zero human sessions**. Human author
qualification remains deferred until v1 preparation after all components exist.

The optimized Mac suite passes **61/61 tests in 13.12s**, including 51 graphical
controller checks for revision grouping, invalidation, conflicts, recovery,
exact integer display, layout/no-op exclusion and explicit version resolution.
The existing offscreen library/outline/inspection view check also passes.

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
  --version 0.4.1-dev.1 --summary 'Correct the reviewed supplier message.' \
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
