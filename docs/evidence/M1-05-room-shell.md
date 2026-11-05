# M1-05 local campaign room shell

Verified on arm64 macOS with Qt 6.12.0 and the optimized desktop build on
2026-11-04. The separate `pgp-game` target opens an explicitly selected local
collection through the typed compiler and scenario loader. Its QML is bundled
with the executable. Engineering content stays in the checkout.

## Implemented behavior

- Office, Lab and Inbox share the date, phase, cash, available funds and
  Play/Pause and Step day controls. Compact layout stacks the inspector beneath
  the room, and the shared scroll view reveals focused controls.
- One session worker serializes campaign operations, saves and preferences. The
  driver uses a 100 ms running interval and stops for decisions, unacknowledged
  liquidity warnings, campaign endings and storage/runtime halts. Required
  choices stay visible across rooms. Core and authored decisions and warning
  acknowledgment use the existing revision-checked session boundary.
- The Lab shows work, commitments and causes. The Inbox reads committed authored
  presentations and saved liquidity workouts. The models reconcile rows by ID
  on the GUI thread and reset on explicit campaign replacement. The view keeps
  a visible anchor ID and pixel offset when the rows change.
- Manual saves and preferences use the existing local persistence layer. Load
  restores the exact checkpoint and holds the external clock. File selection
  accepts absolute local file URLs. Invalid loads preserve the active campaign.

## Focused Mac evidence

```sh
./make.sh --jobs 4
ctest --test-dir build/release -R '^(bridge\.(campaign-model|snapshot-model)|runtime\.liquidity-integration|session\.persistence-policy|persistence\.local-save-settings)$' --output-on-failure
scratch=$(mktemp -d "$PWD/build/release/game-smoke.XXXXXX")
build/release/bin/pgp-game --content tests/content/fixtures/business.json \
  --portable-root "$scratch" --smoke-test --text-scale 200 \
  --screenshot "$PWD/build/release/game-200.png"
```

Five focused CTests passed. The model test includes 200 sorted snapshot changes
with inserts, removals and moves. It checks persistent editing/current indexes,
stable selection and anchor offsets, and absence of routine model resets.

The explicit Metal window smoke passed at 1280×720 and 200% text. It exercised
keyboard room navigation, an accessible Step day control, exactly one paused
step, Play and Pause, and retention of typed filter text, focus and project
selection through at least three running days. It saved, advanced, restored the
exact earlier checkpoint, rejected a missing save with state intact, and
persisted compact layout from the keyboard. The rendered compact Lab was
inspected for text clipping and selected-control contrast.

The smoke calls the same local save/load controller used by the file dialogs.
Live native file-dialog interaction, decision and warning button acceptance,
and scrolling with a populated live inbox remain UI qualification work. The
business fixture starts with an engineering product. Player-facing localization,
research/model progression and recovery-management panels belong to later
scenario work. M1-05 remains open for the M1-03 prerequisite and broader UI
qualification. The accepted count remains **5/52**.
