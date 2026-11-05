# Catalogs, credits and reusable flows

Open a validated collection and choose **Catalogs**. The record list searches
by family, display title or stable ID. The scenario picker selects which
catalogs the record list shows. Shared research, flows and world definitions
appear in each scenario. Choose **Sections** to edit nested fields and
lists, including research prerequisites, recipes, datasets, hardware,
products, executives, board members and rival templates.

The catalog uses the same forms, compiled draft and undo history as the
[world workspace](WORLD.md). Choose an entry from the collection as the
template for an extensible list. Give the new entry its own ID and adjust
its required terms before applying. The shared compiler checks research
cycles, scope, units, starting resources, and dependencies across every
affected scenario. Where values are coupled, stage their sections and apply
the batch together.

## Review an identity change

Select a record to see the typed uses of its ID. The impact list follows
direct uses through other definitions and scenario roots. Follow a result
to open its section or flow. **Review rename** shows each declaration and
reference before applying the whole change as one undo step. A stale review,
duplicate destination or invalid dependent definition refuses the change.

Where a catalog identity appears in several presets, the rename changes that
family across the working collection. Labels, narrative text and provenance
retain their original wording. Registered state and region keys have fixed
simulation identities. A top-level rename introduces a new definition ID at
revision one. Review the corresponding removal and additions in the snapshot
writer. Existing saves continue to use their original content identity.

Use **Deprecate** to retain a definition and its existing references while
marking it in the library. The library also labels flows that are disabled
in every scenario. Scenario enrollment controls when a flow runs. The
unused-record action requires the reference list to be clear and revalidates
the candidate before removing anything from the draft.

## Localized text

Use **New message** or **Edit localized text** to edit the English default
and add translations under the same stable key. Choose `en`, a language such
as `fr`, or a region variant such as `fr-CA`. A translation needs the English
key. An empty localized entry restores fallback. The presentation resolves
the exact locale, then the language, then English. The editor previews that
same lookup. Renaming a key preserves translations and updates its typed
title, body and choice references.

The snapshot writer includes translation changes in its review and in the
revision signatures of affected definitions. The shipped collection uses its
existing English messages.

## Assets and offline credits

**Assets and credits** opens the checkout's existing `assets/manifest.json`.
Register a file under `assets/` or select an absolute local source to copy
into `assets/imported/`. Enter the creator, source, license and its separate
local evidence, purpose, modifications, origin, and offline credit. The
register checks supported file headers and computes the SHA-256. A new
import starts pending. Record completed provenance review before pinning it.
An existing ID retains its file bytes. Give a replacement its own ID.

Saving a record checks the checkout marker and current register and preserves
the previous register as a recovery copy. If an external edit interrupts an
import after its bytes were copied, reopen the register and retry with the
same source and ID to reuse the exact copy. The register preserves source
metadata that is already attached to a record.

**Pin saved record to draft** puts the exact reviewed record in the working
collection. Snapshot review verifies its file and license-evidence hashes and
records the credits in the runtime manifest. Finalization rechecks the asset
observations before committing the batch. If provenance changes, review and
refresh the pin. Removing a draft pin retains the asset and its record.
Selection and installation of the final game art and audio belong to M8.

## Reusable groups

In **Build flow**, choose **Reusable flow groups** and **Copy as group**.
The group owns its execution body and shares the caller's scope. In
**Named group ports**, expose a body variable as an input or output and give
the port a stable name. Inputs read the caller's bound variable. An output
uses the normal assignment effect to update the bound destination.

Select a caller, reopen **Reusable flow groups**, choose the continuation
node and bind every named port. Variables must agree in scope, type, units,
range and visibility. The compiler checks read-only inputs, distinct
destinations, cycles and the existing eight-frame bound. A group can forward
its ports into another group. Open the call's form to inspect the selected
definition revision, the named bindings and its **Continue at** link.

The group's end returns through the existing saved continuation stack.
The finalized collection pins the called definition revision and ordered
body. Delays, dialogue and grouped outputs use the real runtime and save
loader. To exercise a group in Studio, test a caller and inspect the active
call chain and its committed output target. Canvas layout has its own file
and leaves the canonical content hash alone.

```sh
ctest --test-dir build/release -R '^studio.catalogs$' --output-on-failure
build/release/bin/pgp-editor --project /absolute/fresh/checkout-copy \
  --catalog-walkthrough /absolute/new-report.json --text-scale 200 \
  --screenshot /absolute/new-reference-review.png
```

Use a fresh copy with the prepared map and asset register for the explicit
walkthrough. It edits credit in that copy and reopens a separate draft with a
reviewed research rename, localized message and typed group call.
