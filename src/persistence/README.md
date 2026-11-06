# Local save format 1

`pgp::persistence` is a Qt-free adapter above the shared flow runtime. It owns no
network endpoint, identifier, player account, update service or executable
content path. The public interface takes typed checkpoints and explicit local
paths; JSON types stay private.

`encode_save(program, checkpoint)` and `decode_save(program, bytes)` are pure
byte conversions. They neither advance time nor consume RNG. A decoded result
contains an independently validated checkpoint; the caller replaces live state
only after success. `save_file` writes an explicitly selected `.pgpsave` file;
the same operation at a new path implements Save As/Export. `load_file` can also
read a deliberately selected `.bak` recovery file.

The UTF-8 JSON envelope contains:

- `format`: `project-glowing-pancake.save`;
- `identity`: save format, simulation rules, content schema, game/content
  versions and the SHA-256 of the compiler's canonical definition collection;
- `autosave_sequence`: explicit unsigned generation, zero for ordinary saves;
- `checkpoint.world`: one owning core state, scope identities/dates, root seed,
  enabled flow roots, extra registered facts, the authorized cash counterparty,
  decision markers, pending negotiation references and scoped variable values;
- `checkpoint.runtime`: activation/revision/context/current node, start/wait/due
  dates, bounded continuation frames, lifecycle eligibility/count/cooldown/anchor
  state, owned modifiers, effect receipts, presentations/selected choices and
  committed weighted-branch resolutions and duration-predicate observations;
- `sha256`: integrity of the canonical envelope without this field.

Core state includes the exact day/phase/opening cursor, revision, cash,
reservations, due opening actions, pending choice, receipts, next IDs and all
seven named RNG states. Authoritative values remain integer JSON tokens, with
full unsigned 64-bit storage for seed/RNG/IDs. Unknown fields, duplicate keys,
floating/exponent numbers, invalid UTF-8, excessive depth/tokens/counts, invalid
references and unsupported identities fail. The default byte cap is 25 MiB;
callers can select a stricter bound up to a 64 MiB hard ceiling. No original is
deleted or guessed into a future format. Ordinary loads require the exact
game, content and rules identity. Studio can separately decode a supported
finalized revision, apply reviewed typed mappings and validate a copy against
the destination identity. [Revision review](../studio/SNAPSHOTS.md#compare-finalized-revisions-and-review-a-campaign-copy).

The world `variables` array stores explicit overrides of registered defaults.
Each strict record has `binding`, `variable_id` and the existing typed `value`
wrapper; integer, boolean and text values retain their types without coercion.
Runtime validation requires registered IDs, matching scope/type/range and unique
canonical ordering. The bounded array is always emitted, including when empty
defaults apply. Earlier saves may omit it only when the program registry has no
registered variable defaults. Registry defaults are part of the canonical content
identity, so changed defaults cannot silently reinterpret an existing save.

Weighted-branch records retain the activation/node visit, resolved day, flow/node
and branch IDs, bounded sample, and narrative-stream states before/after the
draw. Reload validates those records against the loaded branch weights without
consuming the live stream. A pending delay or choice therefore resumes the
already selected path. `narrative_origin` is null before the first branch and
otherwise retains that branch's starting stream state, so validation can check
the complete history from its first draw. The additive `random_resolutions` and
`narrative_origin` fields are always emitted together. Earlier deterministic
format-1 checkpoints may omit both only when the loaded collection has no random
nodes. Random-bearing content requires an explicit list and origin, including
an empty list and null origin before its first draw. All fields inside a present
record are required and unknown fields still fail. The exact definition hash
continues to prevent loading a checkpoint against changed branch content.

Duration observations retain the definition ID/revision, predicate ID, bound
scope, last observed day and nullable start of its current uninterrupted true
period. The encoder always emits the bounded `duration_states` array. Earlier
format-1 checkpoints may omit it and its paired `duration_sampled_day` cursor
only if the loaded collection has no duration predicate in any trigger or
continuous-exit condition, including disabled and reusable flows. Duration-bearing
content requires both fields even before the first observation: an empty list
and null cursor. Each record has exactly those six fields; its observation day
matches the saved prepass cursor. Runtime validation checks dates, complete
owning roots and the prepared/choice boundary without advancing time or sampling
randomness. The content schema/save format remain development format 1; this
additive reader compatibility does not claim a migration between definitions.

Decoder depth is capped at 64. Its JSON event budget defaults to four million and
can be configured up to eight million. Encoding checks the identical byte/depth/
event bounds before returning bytes, so a newly committed save cannot fail its
own decoder solely because combined record counts exceeded the parse budget.

The current content hash identifies supported normalized definitions and their
version/revisions. It is **not** a shipped asset/localization manifest hash; those
catalogs and finalization arrive in later authoring/packaging tasks. The SHA-256
implementation uses the standard 64-round SHA-256 algorithm with fixed-width
integer arithmetic and is tested against published empty, `abc`, multi-block
padding and million-`a` vectors. Integrity is not authentication or encryption.

## Multi-company history storage

Planned competition saves retain the explicit source scenario, roster settings,
pending entries, weekly decisions/public observations, independent RNG streams,
public first deliveries, research publications/insights, dated prices and shared
monthly adoption. Each company still owns its complete core ledger, capacity,
work, customer and liquidity histories.

Short planned-company `simulation_storage: "history-rows-v2"` stores repeated history
records as fixed-column rows with a per-company `history_strings` table. Only
specified string columns use indices; normal named core records are restored
before the existing typed decoder and deep validation. The reader also accepts
explicit v1 rows and the earlier named representation. Unknown formats/columns,
missing cells, invalid string indices and duplicate table entries fail. This is
lossless storage, with no ledger or work history discarded.

For the v2 tables, expansion is bounded to four million cells and 64 MiB of
referenced string data; each table has at most 100,000 distinct strings of at
most 512 bytes. Input retains the 25 MiB default byte limit and four-million
JSON-event budget.

After 365 settled days, century companies and planned rivals use
`simulation_storage: "history-blocks-v3"`. Command receipts, journal
postings and invoices, daily market and capacity records, adoption
history and rival decisions store their row count and independently
packed blocks. A block contains up to 256 rows
and expands to at most 2 MiB of JSON, including repeated rows. Consecutive
market and capacity days with identical contents share one stored row.
Their dates follow the original close cursor. The reader restores one
block at a time and applies the same typed bounds and checkpoint
validation. Posting IDs follow their original sequence. Each posting
pattern carries its debit, credit, cause and key form. Amount and invoice
offsets refer to the preceding entry with that pattern. Arbitrary keys
stay literal. All historical rows, current obligations and evidence links
survive.

The file still has the configured envelope byte and JSON-event limits.
Current builds read the previous formats. Older readers preserve a v3
save and report unsupported history storage. Full century and sandbox
size and latency remain M8 acceptance checks.

Response-bearing heavyweights also retain named pressure reviews, phase
transitions, warning/delivery/withdrawal dates and funded agreement records.
Restore recomputes observations from public history, replays bounded response
decisions, checks actual paid resource orders, and reconciles escrow, milestone
progress and both sides of each fee. Limits are 8,192 reviews and 512 transitions
per response, 40 agreements and 4,096 public notices. Legacy competition cannot
carry undeclared response/offer state. Capacity saves retain at most 64 dated
staff additions and 64 departures, preserving historical assignments and costs
after withdrawal. [Response contract](../runtime/HEAVYWEIGHTS.md).

Corporate continuations retain the scenario opening cap table, compensation,
cumulative grant/dilution movements, board evidence/votes and warned disputes.
Restore replays ownership and reconciles actual payroll, consideration and debt
maturities. Bilateral acquisitions retain seller allocations, due-diligence and
opening-earnings cursors, control and up to 2,048 actual group transfers per
record. Both payment legs and post-acquisition dividend limits are checked;
group reports eliminate investment assets and internal dividends. The optional
extension preserves legacy scenarios, while exact content identity still gates
development saves. [Corporate contract and bounds](../runtime/CORPORATE.md).

## Local writes and recovery

An existing destination must be a regular file and decode successfully before
replacement. New bytes go to an exclusive temporary sibling, are flushed using
`fsync` or `FlushFileBuffers`, and replace the destination with the platform's
same-directory rename. The previous valid bytes are first flushed/replaced as
`.bak`. POSIX parent directories are flushed too; Windows uses
`MOVEFILE_WRITE_THROUGH`. An existing temporary sibling is retained and refused,
not deleted on sight. Conflicting edits detected during preparation are
preserved. A corrupt/unsupported current destination is refused rather than
silently overwritten; choose another export path or load its valid recovery
copy. Cleanup removes only a temporary file created by this operation.

`AutosaveRing` rotates three primary files. Generations are embedded in saves,
including recovery copies, so losing primary files does not reset ordering.
`latest()` tries current/recovery candidates and returns the newest valid
matching generation while preserving invalid files. A corrupt candidate stops
new automatic writes with an error; it is never discarded to hide a problem.
After recovery, choose a fresh autosave directory or repair the preserved
invalid file before retrying; recovery does not silently resume overwriting it.
Disabled autosaves perform no reads or writes. Monthly saves accept the first
morning, including a suspended required opening choice, or the exclusive
completed final-month boundary. An explicit campaign-end reason also accepts a
valid completed scenario ending midway through a month. The coordinator also
calls the ring before irreversible commitments.

`Settings` has only autosave, reduced motion, alert pause, compact management,
text scale and volume controls. Its separate strict format supports local
backups. Missing settings return defaults without creating files; corrupt or
future settings are reported and preserved. `local_paths()` discovers macOS
Application Support, Windows LOCALAPPDATA, and Linux XDG data/config locations.
An explicit portable root puts saves/settings together without modifying global
preferences. Path discovery creates no directories and records no OS account or
device identity. The game UI will expose those paths in M1-05.
