# M1-04 local persistence implementation

Implemented 2026-11-04. The owning adapter and exact save/settings contract are
[documented here](../../src/persistence/README.md). This implements local IO for
the current foundation/flow state; it does not claim future business fields,
installed shares, objective windows or a complete playable campaign.

The adapter serializes the actual core state and all public runtime checkpoint
fields. It preserves named RNG streams, full-width unsigned values, daily/opening
phase, obligations already represented by reservations/due actions, nested
sequence/subflow frames, activation definition revision/context/current node,
delay start/due, selected dialogue state, effect receipts, lifecycle counts and
anchors, and owned modifiers. A whole checkpoint is validated against the loaded
compiled definition collection before it may replace live state.

Format/rules/schema/content identities and integrity are checked without any
remote lookup or execution. Future/unknown/malformed data fails closed. New
catalogs require their own state/serialization alongside implemented mechanics;
there is no opaque payload for invented future systems. The content hash is of
canonical supported definitions, not an unimplemented shipped asset catalog.

Local writes use exclusive temporary siblings, file flush, prior valid `.bak`
and same-directory replacement, with platform-specific POSIX/Windows calls.
Corrupt originals, pending foreign temporaries and detected conflicting edits
are preserved. Three primary autosave slots use saved generations, including
backup generations when primaries are lost. Recovery finds the newest valid
matching save. Separate settings contain no personal/device identifiers and
support defaults, strict validation, backups and explicit portable paths.

The checked-in test exercises meaningful storage and continuation invariants:

- standard SHA-256 vectors, including the two-block padding boundary and one
  million bytes;
- deterministic whole-checkpoint roundtrip with full unsigned seed/RNG and a
  real suspended core opening choice;
- actual nested two-frame delay, pending supplier dialogue, interval anchor,
  owned modifier, committed receipt and selected-choice continuation;
- identical uninterrupted/reloaded checkpoints for fifteen daily steps, with
  delayed inner/outer sibling markers occurring once;
- modified checksums, unknown/future identities, malformed references/cursors,
  duplicate keys, float/exponent values, invalid UTF-8 and byte/depth bounds;
- initial save, complete replacement, byte-exact prior backup, explicit export,
  foreign temporary collision, corrupt-current refusal, conflict preservation
  and symlink refusal where the host permits creating test symlinks;
- settings defaults/200% text/mute/reduced motion/portable locations, rejection
  of unsupported or identity-bearing fields, and backup preservation;
- disabled autosave, exactly three rotating slots, corrupt-newest recovery,
  backup-only generation continuity, suspended first-morning choices, final-month
  endpoint and midmonth scenario-end autosaving;
- Unicode save filenames through native read, replacement and backup paths.

Parent integration owns the build/CTest runs and records observed results in
the [implementation log](../IMPLEMENTATION_LOG.md). Source-level syntax checking
with Clang, project warnings, `-Wshadow` and warnings-as-errors completed without
diagnostics before integration. Physical power-loss interruption and human save
UI recovery are not claimed from these automated fixtures. The accessible UI
and its standard-path discovery display remain M1-05 work. The existing M0-03
native/deployment prerequisite gate remains separate from this local behavior.
