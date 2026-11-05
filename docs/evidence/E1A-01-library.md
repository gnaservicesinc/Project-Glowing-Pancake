# E1A-01 Content Studio checkout library

`./make.sh --editor` builds the separate `pgp-editor` executable and embeds only
its own view. The regular wrapper profile disables the Studio targets. The
opt-in `ContentStudio` install component currently identifies the executable
for future native deployment. [Build and open the library](../BUILDING.md#browse-a-checkout-in-content-studio)

The library accepts an explicitly selected absolute development checkout or
worktree with a project marker and source tree. It lists approved collection
roots only. `data/runtime/`, `data/drafts/` and `tests/content/fixtures/` have
separate labels. Missing roots are allowed. Paths that traverse symbolic
links are refused. Document bytes, traversal and index size are bounded.
Unsupported checkout schemas are listed without opening the documents.

The shared loader and compiler validate each known collection. The browser
displays ID, kind, revision, source, validation and source fingerprint, and
indexes the actual typed research, flow and effect references in both
directions. It retains a separate canonical semantic fingerprint. Invalid or
future documents stay visible with diagnostic status. No files are created
or changed by opening a checkout.

Apple arm64 optimized editor build passed. The library executable passed 12
checks, including project marker validation, stable ordering, both reference
directions, rejected symlink input, invalid/future documents and exact source
byte preservation. The wrapper passed 12 tests, including the opt-in build
target and no editor targets in the default configuration. The hidden Qt view
loaded 15 rows from three real engineering collections, filtered them to
zero and restored the list without showing a window.

This slice provides a read-only library. Draft changes, undo/redo and conflict
handling remain E1A-01 work. Typed forms, version finalization, interactive
accessibility and native editor deployment retain their own gates.
