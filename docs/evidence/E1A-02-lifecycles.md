# E1A-02 lifecycle and definition forms

The working collection can create a scoped flow with a typed trigger and a
terminal node. Authors can enable it as a scenario root or select a registered
committed fact for a compatible lifecycle. Renaming an existing flow changes
its typed references throughout the same collection. The library and rename
command enumerate the same reference fields. Undo and redo restore the
definition ID and references together. Released content and saves have their
own compatibility gates.

The lifecycle form supports once, recurring, interval, scheduled and continuous
modes. It exposes supported bounds, cooldowns, calendars, anchors and exit
settings. Interval previews call the shared scheduler. A continuous flow
starts with an editable negated entry predicate and distinct timer IDs for its
exit. Changing away from continuous refuses to drop authored exit actions or
modifier registration.

The Apple arm64 optimized editor build passed. The Studio executable passed
103 assertions. Four focused CTests cover the Studio, calendar primitives,
content validation and the flow runtime. The hidden Qt view loaded 15 library
rows. The lifecycle test covers a month-end leap-day clamp, a rejected invalid
date, and an atomic rename across scenario and subflow uses. Execution nodes
and typed effects remain E1A-02 work.
