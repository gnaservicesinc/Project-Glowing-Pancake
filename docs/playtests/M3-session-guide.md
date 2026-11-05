# M3 independent playtest kit

**Status: prepared and deferred; no human sessions are recorded here.** Per the
2026-11-05 user instruction, human testing waits until all components are in
place and the user is ready for v1 work. No sessions or tester recruitment are
underway. The instructions below apply only in that later phase. M3-04 stays
open until five real independent sessions have evidence, including two players
without an ML background and three unaided completions. It does not block
component implementation. Scripted smoke runs do not count. The user or an
explicitly authorized task arranges tester contact when ready.

## Before a session

Use the same reviewed internal snapshot/build for the first five sessions.
Record its game/content versions, content hash and source commit. Do not imply
that this development build is a qualified public release. Build once with the
normal optimized Mac workflow; run the session explicitly with:

```sh
tools/playtest-session.sh S01
```

Use a new ID for each participant. The launcher refuses an existing session
folder, leaves normal local saves/preferences under `.work/playtests/S01`, and
records build/content identity. It collects no gameplay analytics, identifying
data, audio or video. The participant can choose text size, pause, hide/reopen
the guide, save and load through normal controls. Keep the existing guide
available; do not substitute a developer command or a prepared successful save.

Ask whether the participant has an ML background before the session, without
explaining the intended weak subgroup or strategy. Recruit at least two who
say they do not. Use an anonymous session ID in notes; keep contact details out
of reports, Git and shared saves.

Read this consent statement and record an explicit yes/no before observing:

> This is a local development playtest lasting about 30–45 minutes. With your
> permission, the facilitator will write anonymous notes about confusing screens
> and decisions. The game sends no analytics. We will not record audio/video or
> identifying details. You can stop or ask for your notes to be removed at any
> time. May we proceed with those notes?

A no ends the observed study; do not count it as a completed consented session.
Save a copy of the blank [session report](M3-session-report-template.md) under
the ignored session folder. Do not commit real participant notes or saves.

## Participant task card

Give only this task at the start:

> Start a company in the 2014–2017 scenario. Use the in-game information to make
> a plan, develop and evaluate a model, choose a customer promise, and manage
> the business toward the ending. You can pause, inspect, change settings and
> use the guide. There is no preferred strategy or target score for this test.
> Tell us when something is unclear; you can ask for help, take a break or stop.

Do not prescribe a data package, recipe, test, control, financing offer or
headquarters. Do not show the developer baseline/player-path instructions during
the unaided segment. An in-game guide is part of the product and is allowed.

## Observe for 30–45 minutes

Start elapsed-time notes when the task is given. Note manually when the player
first makes a meaningful plan, starts training, reads evaluation evidence,
chooses a promise, accepts a contract, sees a setback, recovers and reaches an
ending. Missing a milestone is a useful result. Do not backfill an inferred time.

Record the screen/action and the player's question, not a speculative diagnosis.
Stay quiet unless asked or an interruption is necessary. Any strategic answer,
pointing to a control, corrective action or facilitator command marks subsequent
completion as **facilitated**; record the first intervention and why. Allow
ordinary app failures to be reported rather than repairing the participant's
campaign invisibly. Safety/consent interruptions end the study without penalty.

At 30 minutes, allow the participant to continue to 45 if willing. At 45 minutes,
stop the timed portion and record current state. An unfinished campaign is not
an unaided completion. A company closure is an ending, but record what the player
understood and whether it reflects a comprehension blocker; do not recast a
scripted or assisted recovery as success. If needed, offer a clearly separated
facilitated exploration after the timed study.

## Ask before explaining

Ask these open questions without supplying the model answer first:

1. What were the main costs using cash, and which commitments kept costing money?
2. What did the evaluation tell you, and how did that affect the promise you chose?
3. Why did your customer contract succeed or fail? What evidence supports that?
4. What happened during a setback? Which responses seemed available?
5. Which decision mattered most? Which screen or repeated action was least clear?

Capture a short answer/paraphrase only with consent. A high score alone does not
prove understanding. Note whether a player distinguishes estimated capability,
independent evidence, task coverage and paid controls. Do not grade agreement
with the fiction's view of AI or a preferred business strategy.

## Review the five sessions

Use the [results template](M3-results-template.md). Count actual consented
independent sessions, non-ML participants and unaided timed completions. Link
anonymous local reports only in private notes. Summarize cash/evaluation/contract
comprehension and recurring issues. Prioritize material blockers, fix them and
rerun the affected path with people before closing M3-04 and release acceptance.
Keep engineering verification separate from human findings. There are
no completed results in these templates, and filling their headings does not
satisfy the gate.
