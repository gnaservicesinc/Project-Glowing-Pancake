# Scenario and world authoring

Open a validated scenario in the library, or choose **Scenario and world**
from a working collection. Select a preset, search for a section or field,
and edit its structured fields. The current schemas cover opening dates,
seed, cash, compute, team, ownership, bills, research and enabled flows;
rivals and their planning/response policies; regions, customers and product
channels; site profiles, equipment, travel and resilience; financing,
governance and management policy defaults.

**Management → Scoring** edits the implemented slice's useful-work target,
five score weights and founder baseline. Century objectives will enter this
workspace with their owning M7 simulation schemas.

Fields come from the compiler's normalized typed data. Booleans and known
references use pickers. Integer input stays exact through 64-bit seeds;
money uses constant-2014 cents, exposure uses basis points, and compute,
power, water and timing retain named units. Fixed profile/weight arrays keep
their schema-defined shape. Each section is checked by the same content
loader and compiler used by the game.

## Apply related fields together

The form shows each field's before/proposed value. **Apply section** validates
the whole section as one document change. This permits transfers between
ownership entries or changes to several score weights while preserving totals.

For constraints across sections, choose **Stage section**, adjust the related
section, and use **Apply staged sections together**. For example, changing
the founder's opening share also needs the matching scoring baseline.
Moving a campaign date may need changes to offer and incident windows.
Up to 64 sections can form one atomic batch and one undo boundary.

Staged and unapplied values remain outside the compiled draft. Save and
finalization require applying or explicitly discarding them. An outside
document edit preserves pending values as a visibly stale batch; it cannot
apply against the new revision. Rejected changes retain the working draft,
pending fields and its undo history.

## Create and navigate

Duplicate a scenario under a new stable ID to create a complete preset.
**Add missing systems from template** copies absent sections from another
validated preset in the same collection, retaining existing settings.
Root controls can disable optional systems as a validated batch.

Select an extensible list, choose a typed entry template and **Add from
template**. Adjust the new entry's identity and terms before committing it.
Empty reference lists use the owning vocabulary; empty object lists can use
matching records from the collection. Opening contracts use actual starting
products and customers. Removal checks every dependent reference. Existing
definition identities and revisions retain their owning refactor/finalizer
contracts.

The current Lab starts with an empty model/evidence history. Author its
deployable products under **Service catalog**; campaign training, evaluation
and deployment create their measured records. Fixed starting-market fixtures
use their own supported starting-product/contract schema.

**States map and list** uses the same prepared, credited offline Census
geometry as the game. Pointer selection and the keyboard list open the same
state profile. The asset ID and hash must match the selected scenario for
the map to display; the list remains available. Shared regional and resilience
definitions appear as separate roots and affect all referencing scenarios.

## Inspect and save

Choose headquarters and **Inspect opening and exact save**. This creates a
real isolated scenario, prepares its opening and roundtrips its complete
checkpoint through the game's save codec. The report shows registered facts,
actual target bindings, trigger clause truth and the enabled flows' effect
targets. Member-scoped incidents require real members/outcomes. Inspection
does not change the document or a live campaign.

Save under `data/drafts/`, reopen, then use the existing **Review content
revision** workflow to inspect/finalize a batch. Draft editing retains runtime
content, component versions and definition revisions. Snapshot validation and
external-write conflict protection are shared with flow authoring.

```sh
ctest --test-dir build/release -R '^studio.world-forms$' --output-on-failure
build/release/bin/pgp-editor --project /absolute/fresh/checkout-copy \
  --world-walkthrough /absolute/new-report.json --text-scale 200 \
  --screenshot /absolute/new-map-and-fields.png
```

The explicit engineering walkthrough submits real QML fields, stages coupled
ownership/scoring edits, rejects decimal/stale input, selects states through
keyboard and map hit tests, adds financing, inspects the actual opening,
saves/reopens a separate draft and checks the 900-pixel workspace. It preserves
the copy's runtime content and versions. Human author studies remain deferred
to v1 preparation.
