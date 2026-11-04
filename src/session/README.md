# Campaign persistence policy

`pgp::session` owns an executable runtime and its local autosave policy, above
the Qt-free runtime/persistence libraries. The application supplies a selected
autosave directory, using `persistence::local_paths()` for OS-standard or portable
locations. Construction creates no files. All campaign commands pass through
`Runtime::submit`, which enforces morning and required-choice boundaries.

The owner first validates an action on a copied runtime. Refused actions perform
no storage writes. Accepted irreversible reservations, reservation consumption,
core opening choices and flow choices save the current state before publishing
the candidate. If that recovery write fails, the original campaign remains
current and the action is refused with a storage notice.

Advancing into a new month saves the completed day's new boundary, including a
suspended required opening choice. Reaching an exclusive scenario ending saves
the final state even between month boundaries. If either write fails, the
completed day stays current; the result requests a driver halt and retains an
explanatory notice. Disabling autosaves prevents these reads/writes explicitly.

Drivers must stop automatic stepping on `halt_requested` and deliver storage
notices independently from coalesced snapshots. Failed automatic opening/day
operations request halt; ordinary stale or invalid player commands do not
require a timer halt. This prevents an unchanged failing day from being retried
forever. The QML room-shell integration remains M1-05.

Manual save/export uses the same validated atomic writer. Loading constructs and
validates a complete candidate before replacing live play; a failed load retains
both the current campaign and the selected file. Preferences are validated here;
the explicit settings file reader/writer belongs to `pgp::persistence`.
