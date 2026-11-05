# E1A-01 typed draft workflow

A selected validated flow opens a working copy in the existing Studio.
Adding or removing tags uses the typed collection and shared compiler.
Sixteen undo steps retain canonical states. Undo and redo work after saving.
No-op edits and no-op in-place saves preserve the existing history and file
bytes. Content version and existing definition revisions stay fixed.

The author explicitly saves a new lowercase filename under `data/drafts/` or
saves an opened draft in place. Reviewed and engineering input files remain
unchanged. Before writing, the Studio checks the approved paths, checkout
marker and exact source bytes, including changes that leave the semantics
unchanged. An existing target cannot be adopted by Save As. A removed draft
cannot be silently recreated. In-place replacement uses a flushed temporary
sibling and preserves the prior file in a recovery copy. Unrelated pending
temporary files are retained and cause the save to refuse. Tested symbolic
parents are blocked. Conflict evidence covers external changes before saving.

Apple arm64 optimized Studio build passed. All 38 Studio assertions and the
focused local-save/settings and session-persistence CTests passed. The hidden
Qt check loaded and filtered 15 rows and ran an in-memory tag edit, undo and
redo. No working content was written to the real checkout in that check.

This is the initial metadata edit surface. Full trigger, lifecycle, node and
effect forms belong to E1A-02. Multi-file finalization, interactive author
qualification and native editor deployment retain their own gates.
