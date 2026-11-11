# Life, innovation and development tools

Selected by the user on 2026-11-11. This extends the active people/software
plan and supersedes its victory conditions and age-118 ending.

## A life and its consequences

The founder dies at age 100. For the current 1997 birth, the retrospective is
dated 2097. A newspaper edition reports the founder's life, the actual state of
the world and their recorded contribution. It covers their company, the other
major labs, public opinion, named personal relationships, family, choices and
the consequences that actually occurred. Historical reports identify their dates.
No victory, defeat, score threshold or winner is assigned to the player.

Mission statements and bylaws remain consequential commitments. They affect
public response, applicant interests, compensation expectations and morale.
Nonprofits receive goodwill and free public attention, corporate income-tax
exemption, and applicants who willingly accept lower compensation because they
support the mission. Reaction should reflect whether the institution follows
through. Salary satisfaction uses each applicant's actual expectations.

Closure, dismissal, loss of control and nationalization remain story changes.
The career and USCIF work in the people/software plan must preserve the person
and their record through those transitions. An early company report must never
claim that a living founder died.

The user also selected voluntary resignation. Offer a reviewed departure with
a handover choice, disclosed employment obligations and dated reactions.
Distinguish the executive job, a board seat and share ownership, so leaving one
role does not silently dispose of another. A retained shareholding can provide
its actual future dividends. Return to the founder's home, with the family
bedroom available, and offer ordinary formation and competitor-employment routes.

Career transitions include resignation, board dismissal, AI replacement,
retirement or delegation, bankruptcy/closure, acquisition or merger and
nationalization. Capture the actual reason, handover, compensation/ownership
outcome and personal reactions in the biography. A successor's company keeps
developing in the world. Personal wealth, family and relationships follow the
person, and the retrospective can report what happened after the departure.

### Resignation statement and the story it creates

Before resigning, the founder builds a press statement from a chosen reason,
handover, successor and PR angle. Offer selectable, editable statement sections
with a preview so the player can frame the announcement. Possible angles include
leaving on a high, completing a mission, making room for a new generation,
accepting responsibility, personal priorities and a disagreement over direction.
Preserve what the founder actually said, including claims that overstate events.

At announcement, capture the supporting company record, including finances,
delivery, research evidence, mission/bylaw commitments, scandals, board disputes
and workforce reactions. Reporters compare each factual claim with dated
evidence. A confident statement can be questioned or contradicted by that
record. Opinion and spin remain distinct from claims that can be checked.
Press coverage should identify the particular fact or inconsistency, and
reputation changes follow the public response and subsequent events.

An unexpected successor can take over the coverage. Offer appointments supported
by the normal governance process, including surprising internal or outside
candidates. When this company has an independently established ASI, that ASI
is also a handover option. The plan must record whether the founder is proposing
the appointment or can authorize it, its actual approval and the control rights
being transferred. Existing independent capability/control evidence informs
the resulting coverage and consequences.

Choose the report's lead from the actual announcement and circumstances.
A startling successor, weak control arrangements, a contradiction in the
statement or a scandal can dominate the story despite the player's preferred
angle. Different audiences may respond differently. Save the statement,
fact checks, actual appointment, media focus and later handover results so
the obituary can revisit that turning point accurately.

### A continuing path to age 100

Every early company ending or forced departure must offer at least one executable
way back into play. The player can accept a context-appropriate CEO appointment at
an operating major lab, another suitable funded role, or a special scenario such
as USCIF. The offer must work with the actual world and resources, including
worlds where the original company has failed. It cannot depend on spare personal
cash that the founder no longer has. Preserve the appointment's salary, equipment,
governance, relationships and future expectations as ordinary simulation state.

The route depends on the event and record. Reinstatement can follow a dismissal,
a competitor can recruit a displaced CEO, and government intervention can lead
into the selected civilian-administrator scenario. Voluntary resignation instead
opens the player's choice of the next chapter, including home, a new company or
employment. Forced career events pause for a playable next chapter.
As long as the player continues, they can reach 100.

Resignation explicitly offers Continue this life or End here and read the
retrospective. Ending voluntarily produces a dated career feature using the
founder's current age and record. It does not claim a death. Save that choice
and the final report boundary. The age-100 ending remains the obituary.
Neither path assigns a victory or defeat.

Retire the former perfect-utopia result. World reports describe particular
benefits, costs, distributional effects and disagreements using the simulation's
actual evidence. Broad improvements can coexist with dependence, concentrated
control, environmental costs, displacement and unmet needs. They do not collapse
into a single universal morality score. Avoid inventing a downside just to cancel
a positive result, and allow some outcomes to be uncomplicated successes.

## Research and innovation

Selected technologies have authored, finite innovation levels. The first
completion is research. Subsequent levels are presented as innovating.
Only eligible technologies offer that action. Each level defines its own
cost, staff/equipment/compute needs, effects and next-level limit.

Prerequisites can require a particular level, including a level of another
technology. Preview, commands, saved completion history and the technology tree
must agree. Effects apply once per completed level. Repeated research must
never accidentally duplicate first-completion rewards. Rivals use these same
rules. Well-understood algorithms are candidates for incremental improvements,
including sorting, with bounded gains appropriate to their actual workloads.

## Distribution and development builds

Add --enable-distribution and --disable-distribution, with explicit boolean
forms. Distribution requires Release and configuration errors for another mode.
Release alone remains a normal development build. Distribution excludes the
development menus and their callable implementations.

Development builds expose two menus whenever their tools can be used.

- Content Studio opens the repository authoring application with organized
  browsing, visualization, creation and editing for every event type. Resolve
  the checkout beneath /opt/AI Startup first. If it is unavailable, open a folder
  chooser and remember the selected checkout in per-user application settings
  under Library on macOS. Provide a way to change it. Authoring writes repository
  content for the next game build.
- Playtest Environment is a tool launcher. Its first tool is God mode. It can
  edit founder traits and any generated person's traits and stats, provide
  unlimited resources, deliver orders immediately and complete technologies
  through an instant research button. These operations use the real systems
  so staffing, equipment, effects and dependent state stay consistent. Ordinary
  game saves and playtest sessions must clearly identify their development state.

## Small learned rival policies

The user confirmed the intended rival ambitions. A rival seeks first ASI,
then meaningful control of it, then a distinct secondary ambition derived from
the former objective set. Success requires the combination. These ambitions
shape rival decisions and their training rewards. They do not reintroduce
player victory conditions.

Use compact decision networks as the first candidate, targeting roughly four
million parameters. Structured observations contain the information available
to that company, relevant history, resources, staff, technology/innovation
levels, relationships, public evidence and its ambition. Variable rosters and
candidate actions use bounded entity slots and masks. The output selects a
legal action and its arguments, using the same validated commands as other
companies. Private rival state and future random draws stay outside observations.

Begin with imitation of recorded feasible planner decisions, then train a
population against current and historical policies, scripted opponents and
different seeds. Use intermediate progress signals and a combined final
objective so ignoring ASI control cannot be rewarded as success. Hold out
world seeds, opponent populations and policy checkpoints. Evaluate behavior,
not only reward, including broken economies, inactivity, collusion, illegal
actions and repeating a convenient exploit.

### Carry forward LoomFactory's native graph lessons

The user observed a large speed improvement for small models in LoomFactory
after moving away from Python to native custom graphs. This is the starting
architecture decision for this work. Host overhead can dominate a small model's
actual computation, so elapsed end-to-end throughput governs implementation.

Build the trainer, environment stepping and policy inference in native code.
On the initial Mac target, use Metal/MPSGraph from a narrow native bridge and
construct reusable forward, gradient and optimizer graphs. Keep weights,
gradients and optimizer state GPU-resident across steps. Batch environment
observations, action selection and training updates. Avoid interpreter workers,
per-actor process launches, per-step full weight transfers and per-operation
CPU/GPU waits in the training or inference path.

The local references are LoomFactory's NativeTrainingSession,
NativeOptimizerGraph and compact network implementation under Sources/LoomNative,
plus Docs/NativeTraining.md and its measured throughput reports. Inspect those
contracts before reuse. Share architectural lessons and adapt the actual policy
network and losses to this game. The image network is a different workload.

Benchmark the complete rollout/update pipeline, including simulation stepping,
packing, dispatch, synchronization, gradients, optimizer and checkpointing.
Report warm-up/compilation separately and measure the intended small batches as
well as larger batched workloads. Compare numerical outputs, gradients,
parameter updates, saved/reloaded optimizer state and gameplay decisions before
accepting an optimization. LoomFactory found graph/encoding shortcuts that
failed real-size gradient checks, which makes numerical parity part of this
implementation work.

Retain immutable checkpoints, lineage, seeds, observation/action schemas,
reward definitions and opponent identities. A compact parameter count is an
experiment target. Training results and throughput require actual measurements
on this game. Native CPU inference provides a portable fallback, and distributed
gameplay remains offline with bundled validated weights.

Method references include the PettingZoo multi-agent environment and action-mask
contracts, the original Population Based Training paper, and the multi-agent
autocurricula experiments.

- [PettingZoo multi-agent environment API](https://pettingzoo.farama.org/)
- [Population Based Training](https://arxiv.org/abs/1711.09846)
- [Emergent Tool Use From Multi-Agent Autocurricula](https://arxiv.org/abs/1909.07528)

## Delivery sequence

1. Replace the player result screen and scored endings with the retrospective,
   change the founder lifespan, and verify the report against actual records.
2. Add the distribution switch, repository locator and two development menus,
   then implement the complete God mode workflow.
3. Add selective innovation levels and level prerequisites through ordinary
   research, event, rival and save entry points.
4. Complete mission/nonprofit reactions and the career transitions already
   selected in the people/software plan.
5. Build the native policy environment and measured graph baseline, then train
   and compare the rival population. Integrate validated policies into gameplay.

The existing infrastructure puzzles, specialist effects and remaining
people/software work remain part of the active goal.
