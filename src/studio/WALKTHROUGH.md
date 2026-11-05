# Forms and inspection engineering walkthrough

E1A-05 demonstrates the narrow authoring path in the runnable Mac editor.
`pgp-editor --walkthrough REPORT.json` uses the actual QML control handlers,
typed forms, keyboard outline, copied-state inspector and draft save/reopen
path. It is an explicit development check, separate from the deferred human
player/author studies. Its report always records zero human sessions.

## Run on an isolated checkout

Build the editor once and retain the optimized build. Create a fresh detached
checkout copy so the demonstration never changes the working runtime content:

```sh
./make.sh --editor --jobs 4
git worktree add --detach .work/studio-walkthrough HEAD
build/release/bin/pgp-editor \
  --project "$PWD/.work/studio-walkthrough" \
  --walkthrough "$PWD/.work/studio-walkthrough-report.json" \
  --screenshot "$PWD/.work/studio-walkthrough.png"
```

The application opens at 1280×720, performs the engineering path, captures the
inspection view and exits. It refuses existing report/screenshot files and an
existing `data/drafts/e1a05-walkthrough.json`. Use new paths for a later run;
existing drafts/results are preserved. `--walkthrough` requires an explicit
`--project`; `--screenshot` requires the walkthrough. A build never runs it.
`--software` may be selected explicitly, but software success does not prove
Metal/device behavior. The observed Mac run uses Metal.

The output includes one new draft, a local JSON report containing the actual
before/after/queue traces, and the optional PNG. It performs no finalization,
Git operation, publication, analytics or tester contact. Ordinary authors use
the same visible controls; this command is an engineering convenience.

## Authoring path and expected evidence

1. Search the current library and open a validated slice flow. Inspect the
   collection source, revision, uses and used-by information.
2. Create `walkthrough.branching` in company scope. Enable it for the scenario.
   Build an ordered sequence with a cash effect, a decision marker, a two-day
   wait, branching dialogue, distinct accept/decline results and a delayed hidden
   marker on the accepted branch. Every edit goes through typed form controls;
   no collection JSON is constructed or edited by the walkthrough.
3. Plant a **+250-cent** effect where a cost is intended. A preceding actual
   **1,000-cent debit** funds the counterparty pool, preserving cash conservation.
   The copied-state receipt shows the exact company target and +250 delta;
   the total opening debit is 750 cents. Correct the effect to **−250 cents**
   through the form. The receipt and ledger now show a 1,250-cent total debit.
   A document edit clears the old inspection fixture before another run.
4. Check that the cash-target picker excludes world scope and that the typed
   form refuses a wrong-scope value with an explanation. This is an engineered
   invalid-binding check; it is not evidence of a person's comprehension.
5. Change the trigger to cash ≥ 0 and inspect below/equal/above threshold facts
   on isolated copies. Inspect the real continuation frames and due date.
6. Add recurring, interval, scheduled and continuous examples using their
   lifecycle forms. The branching example supplies once. Save with **Save as
   draft**, close it and reopen it from the library. Select all five through
   the working-flow picker; verify each saved mode, run an isolated fixture
   and reload through the game save codec. The continuous example enters
   naturally. These narrow examples complement the more detailed lifecycle
   entry/tick/exit/cleanup regression fixtures.
7. Return to the branching flow, reload its wait, advance to 2014-01-03, accept
   through the shared dialogue button, reload again, then advance to 2014-01-04.
   Its hidden consequence executes once; the unchosen result does not appear.
8. Verify that normal form/save/inspection actions left reviewed runtime source
   and root version bytes unchanged. Only the separate new draft was saved.

The recorded run passes **36 QML form submissions**, five lifecycle reopens,
scope/sign checks and exact delayed continuation. The path caught and fixed
the Save as draft button's invalid JavaScript string call (`trimmed` → `trim`),
which direct document-model tests had not exercised. Named controls provide
stable engineering targets without changing the author-visible labels.

## Validate the saved batch separately

The E1A-04 maintainer writer validates/promotes the resulting draft in the
isolated checkout. The graphical finalizer remains E1B-02:

```sh
mkdir -p .work/studio-walkthrough/.work
build/release/bin/pgp-content-tool review \
  --checkout "$PWD/.work/studio-walkthrough" \
  --source data/drafts/e1a05-walkthrough.json \
  --version 0.2.0-dev.1 --summary 'Reviewed engineering authoring walkthrough.' \
  --plan .work/walkthrough-review.json
# Read the report before running finalize:
build/release/bin/pgp-content-tool finalize \
  --checkout "$PWD/.work/studio-walkthrough" --plan .work/walkthrough-review.json
build/release/bin/pgp-content-tool check --checkout "$PWD/.work/studio-walkthrough"
```

The example version is newer than the current 0.1.0-dev.1 baseline; choose a
newer version if that baseline advances. The observed isolated batch introduces
five revision-1 flows and increments only the enabled scenario to revision 2.
All three headquarters openings/save roundtrips pass, with no compiler lints.
Its isolated content 0.2.0-dev.1 hash is
`be4e89994465d11bac181f18c81e645b1e9e8ea1bbeae4802cdbed7d06f902ef`.
The primary checkout/game stays on the reviewed 0.1.0-dev.1 snapshot. Draft bytes
remain untouched by promotion; the required game bump stays unresolved.

The generated game resource lists contain only game QML and the reviewed runtime
collection/manifest. Studio QML and the walkthrough are compiled into the
separate editor target. Drafts, layouts, immutable authoring baselines, reports,
journals and editor binaries are excluded from the normal game target. This is
Mac development evidence; clean native package/device qualification remains
deferred. The canvas, graphical finalizer and later human usability acceptance
remain their own tasks.
