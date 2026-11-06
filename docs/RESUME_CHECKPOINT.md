# Paused implementation checkpoint

Paused at the user's request on 2026-11-05. Do not resume the persistent goal
until the user asks. This checkpoint preserves unfinished work; it does not
complete a backlog item or finalize content.

## Verified progress

- **37/52 steps complete; 15 open.** The authoritative completed baseline is
  `main` at `3a81891587b6fb978dfc0c0513fcb9311414fc19` (M6-02).
- M6-03, executive/public conflicts and authority paths, is in progress and
  remains open. Subsequent implementation begins with M7-01 after M6-03.
- Preserve M3-04/E1B-03 as deferred human gates. There are no available testers;
  the user wants all components in place before v1 human testing. Do not recruit,
  schedule, or repeatedly request testers. The existing kit remains available.
- Native matrices, packaging, device qualification, release acceptance and
  publication are separate later gates. The percentage is the roadmap count,
  not a claim of release readiness.

## Work preserved on the checkpoint branch

`wip/m6-03-pause-2026-11-05` contains all M6-03 source and draft changes.
Keep `main` at the completed baseline until the feature is implemented and
verified. Continue on this branch, then integrate one cohesive completed feature.

Implemented source, not yet behavior-verified:

- Typed authority catalogs, state, actions and strict JSON codecs; optional
  scenario/save ownership and shared Content Studio definition references.
- Public testimony, finite publication/service commitments, public disputes,
  funded negotiations and source-derived audience identity.
- Four institutional roles: standards, infrastructure, delegation and abstract
  coercive authority. Named regional issuers, real fees, paid contract teams,
  actual staff/compute work, review dates, expiry, withdrawal and revocation.
- Real procurement eligibility, resistance, public sanctions and regional
  authority/coverage histories. Money, advertising and market share alone do
  not grant authority.
- One funded extension of a concrete executive publication dispute, preserving
  the existing governance/departure source records.
- Runtime transaction/save integration and event-date source replay validation.

Main owning files: `src/core/authority.cpp`,
`src/core/include/pgp/core/authority.hpp`, `src/content/authority_catalog.cpp`,
`src/content/authority_codec.hpp`, `src/runtime/authority.cpp`,
`src/runtime/authority_queries.cpp`, `src/runtime/authority_validation.cpp`,
`src/runtime/authority_internal.hpp`, and
`src/runtime/include/pgp/runtime/authority.hpp`.

`data/drafts/m6-03-institutions.json` is a **draft** based on the current content.
It links all five scenarios to the new catalog and three institutional news
flows. The shipped content and component manifest have not been finalized or
version-bumped. Use the shared content review/finalize tool later; do not
manually manufacture definition revisions or content identity.

## Verification limits and immediate next work

An earlier incremental Release build completed, but later source edits have
not been rebuilt. `.work/m6-03-build.log` is historical output, not proof of the
current checkpoint. No M6-03 focused behavior tests, UI checks, or full suite
have run. No build/test/game process was running when this checkpoint was made.
The completed M6-02 baseline previously passed its 74-test suite and focused
Mac/UI checks; those results do not validate M6-03.

1. Read this checkpoint, `AGENTS.md`, `docs/STATUS.md` and the M6-03 backlog/spec
   sections. Inspect branch and status before editing. Keep optimized Mac build
   caches. Do not count M6-03 as complete yet.
2. Add focused real-runtime authority tests using the draft: paid regional
   applications and actual public delivery; testimony and finite commitments;
   concern/formal/negotiation/deadline outcomes; procurement; coercive resistance
   and sanctions; executive negotiation; exact reloads and forged-source
   rejection. A compiler pass alone is insufficient.
3. Check these unresolved boundaries in the implementation:
   - Align case cure versus disclosure exactly on the response deadline.
   - Reject forged institutional project cancellation unless a real withdrawal,
     expiry, denial, revocation, executive departure or terminal cause owns it.
   - Validate service commitments against the actual region and eligible public
     customers, plus historical active-commitment limits.
   - Exercise same-day publication/cancellation ordering in source replay;
     preserve the pinned original deployment and previous-day review cutoff.
   - Keep the procurement predicate and market-influence predicate consistent.
   - Verify bounded daily work and avoid full-history copying/validation inside
     daily loops. Existing full-suite timing was about 98 seconds, so the
     repository's few-seconds target has not been achieved.
4. Finish session integration: unread public cases must halt advancement and
   surface a concrete acknowledgement; new irreversible institutional actions
   need the existing autosave boundary. **The last proposed patch to
   `src/session/campaign.cpp` was not applied.** That file is unchanged at pause.
5. Finish the Institutions controller/panel/tab, six-audience identity view,
   funded-action previews/confirmation, named grants, commitments and cases.
   Respect `snapshot.driver_halted || core.sim.paused`; keep business rules in
   the owning backend, not QML. Add focused native Qt UI smoke checks.
6. Finish Studio role-record references, fixed four-offer and identity-matrix
   editing/help. Existing authority root references are wired, but the last
   proposed role-family/matrix/help patch was not applied. Optional-society
   fixture disabling must also clear the new `authority` scenario reference.
7. Once behavior and data are stable, finalize the shared content batch through
   the owning tool (planned content 0.10.0-dev.1, generation 10), rebuild
   incrementally, run meaningful focused checks and relevant regressions.
   Perform native Mac UI checks at 100%/200%. Defer human/platform gates.
8. Only then update completion to 38/52, write the owning module documentation,
   update completion records once, commit/push the cohesive feature, verify
   remote HEAD/status, and continue with M7-01 when the user resumes the goal.

No automatic continuation, background testing or release action is requested
while paused.
