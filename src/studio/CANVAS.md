# Typed flow canvas

E1B-01 adds a node canvas to the separate Content Studio. Canvas, outline and
settings project the same `DraftDocument`; every logical change passes the
shared compiler and undo/save boundary. There is no saved second graph language
or preview interpreter. The primary runtime snapshot remains 0.1.0-dev.1.

## Authoring controls

Open a checkout and edit a flow. **Build flow** shows the canvas beside its
selected-box settings; **Outline** exposes the same rows and fields. **Browse
library** retains source, revision, uses and used-by navigation. Narrow windows
switch between **Canvas** and **Settings and test**. The current flow/content
version remains visible.

- Find a box in the searchable navigator and choose **Focus**. Select a box or
  numbered port to edit its actual form. Inputs/outputs name their kind, unit
  and scope; colors supplement those labels.
- Select an output, then a compatible input. Execution, predicate, value and
  entity ports cannot be coerced. Value type/unit/reference family and binding
  scope must match. Compiler rejection preserves the previous document.
- Choose a port, search its palette and press **Create**. Execution nodes are
  inserted on an existing route, keeping its continuation. Predicate groups
  and supported value expressions use the same typed forms. Add/reorder effects,
  numbered sequence children, choices and weighted branches in their settings.
- Retargeting an execution route removes only nodes no longer reachable from
  entry/exit. **Undo content** restores the entire change. Shared continuations,
  cross-sibling boundaries, bounded delay loops and subflow recursion are still
  checked by the runtime compiler.
- Predicate/value inputs own their trees. Connecting copies the selected tree;
  it does not create a hidden mutable shared expression. Duplicate duration IDs
  are repaired; connecting an ancestor into its descendant is refused. Registered
  subflow references provide actual reusable execution with a saved return.
- Literal-only effect arguments accept compatible entered values; they cannot
  silently sample a live fact. Typed expressions remain available for comparison
  operands and variable assignment where the registry/runtime supports them.
  Atomic effect members have fixed order links; their form controls edit that
  order without implying separate/concurrent transactions.

Drag a box header or focus the canvas and use arrows to move a selected box by
16 logical points. Dragging previews geometry locally; release commits one
integer layout move. **Undo move**/**Redo move** are separate from content undo.
Drag empty space or use Ctrl+arrows to pan; use the wheel or zoom buttons for
10–200% canvas zoom. **Fit all**, **Fit selected** and **Arrange** never rewire
content. Port buttons and the navigator/palette also work through keyboard focus.
Escape cancels a pending connection; Home fits the graph.

## Runtime inspection

Start an isolated scenario test using the shared inspector. Cards/ports show
current clause values and truth, resolved scopes, actual effect receipts,
selected choices/weighted outcomes and saved continuation state. A waiting
sequence shows the current child, queued siblings and the queued return route.
Compiler notices explain delays/choices before later numbered children; their
full text opens in a keyboard-accessible notice view.

Green labels identify actual committed/true/active observations; amber identifies
waiting/queued work; red identifies false or rejected candidates. Synthetic
separate-copy operations remain explicitly purple/labeled. Mixed member truth
does not become a fabricated single result. The full inspector retains target,
before/after, ledger and queue details. Graph projection/inspection does not
advance a campaign, consume RNG or predict an unexecuted random branch.

## Layout and local state

| Artifact | Rules |
| --- | --- |
| `authoring/layouts/<flow_id>.layout.json` | Shared format-1 layout with flow ID, grid 16 and integer node coordinates; written only by explicit **Save layout** |
| `.work/editor-session.json` | Ignored local selection/pan/zoom per collection + flow; viewport zoom is stored as integer permille |
| Runtime collection/draft | Existing typed logical source; changes only through a validated content edit/save |

Opening a graph does not create a tracked layout. Coordinates are bounded to
0–1,048,576 on the 16-point grid; local pan is separately bounded and zoom is
100–2,000 permille. No float tokens enter shared layouts. Layout-only gestures
do not dirty content, alter its collection hash, increment definitions or bump
game/editor/content versions. Unsaved moves survive switching flows in the
working session; close/discard protects outstanding content/layout edits.

Layout writes use the existing atomic-file/backup primitives, original-byte
conflict checks, bounded/duplicate-rejecting metadata and checked local paths.
Stale, linked, corrupt or future-format files are preserved. An unsupported
layout can fall back to automatic display while shared layout writes remain
blocked. Explicit reload discards only local unsaved moves after its dialog.
Content batch finalization remains the journaled E1A-04 writer; its graphical
review/finalize wrapper is E1B-02.

Default layout is a stable bounded layering of the projected graph, with no
overlapping boxes in a layer. Positive-delay cycle handling breaks a layout
back-edge only; it never changes executable links. The canvas has a bounded
4,096-box/16,384-port projection, with the shared outline as fallback. Wires draw
into a viewport-sized surface rather than allocating an image as large as the
logical world. Broader catalog/group tools and large-content qualification
remain E2 work.

## Reproduce the engineering check

Use the retained optimized editor build and a fresh disposable checkout:

```sh
./make.sh --editor --jobs 4
git worktree add --detach .work/studio-canvas HEAD
build/release/bin/pgp-editor --project "$PWD/.work/studio-canvas" \
  --canvas-walkthrough "$PWD/.work/studio-canvas-report.json" \
  --screenshot "$PWD/.work/studio-canvas.png"
ctest --test-dir build/release -R '^studio\.(canvas|library|snapshots)$' --output-on-failure
```

The explicit walkthrough extends the E1A-05 example using real QML controls and
pointer delivery: typed port refusal/connection, palette creation, shared undo,
actual queued siblings, pointer-drag release, keyboard grid moves, layout save/
reopen and ignored viewport restore. It checks narrow-window navigation and
200% canvas zoom without content/layout changes. Existing draft/report/image
outputs are refused. Builds never launch the editor or run this command.

Mac evidence: **84 canvas assertions**, **60/60 optimized CTests in 12.18s**,
and rendered **Metal at 1280×720** with actual pointer ports/dragging. The game
resource lists still contain only game QML and reviewed runtime data; all canvas
resources stay in the separate editor target. These are engineering checks.
Human studies remain deferred until all components are in place and the user is
ready for v1 work; no author usability, screen-reader, other-device or native
package qualification is claimed.
