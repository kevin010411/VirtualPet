# Death runtime implementation

The authoritative product and binary contracts remain in the paired Web
repository's `.scratch/sd-driven-pet-stats/spec.md` and
`.scratch/binary-runtime-tables/spec.md`. This note describes firmware execution.

`Pet` owns the persistent Dead bit (`flowFlags & 0x4`) and the active appearance's
volatile selected-Stat mask. Stat commits complete atomically, then evaluate
death. Once Dead, Stat mutation rejects further care; disabling the predicate
does not clear the saved bit. Save version 18 retains the 104-byte state layout.

`Game` cancels pending startup, evolution, collection, appearance selection and
Guess Game work when death is first presented. Its base animation selects the
resolved death animation, or the existing default idle when a disabled project
has no death asset for the active appearance. Playback uses the existing base
rotation/loop mechanism, does not enqueue repeating actions, and does not refresh
the platform's interaction clock. Normal command slots are suppressed.

The death snapshot bypasses collection evaluation and RTC save batching. Failed
writes retain Dead in RAM and retry at most once per second in the ordinary game
loop. Low-battery handling continues to suspend that loop and input. Until an SD
write succeeds, power loss can still recover the preceding successful save.

Reset constructs initial state under the initial appearance's runtime contract,
recomputes initial grants, and saves only its final snapshot. A failed attempt
restores the preceding Pet and appearance. A storage failure leaves the death
screen ready for a new button attempt; a required resource/contract failure uses
the existing fatal resource policy. A/B candidate writes retain the preceding
successful slot. Successful reset clears the save batch, daily pauses, clock
backlog, collection pending flag, menu selection and all transient gameplay.

Platform input observes time before buttons. A newly entered Dead state consumes
the current held gesture. `ButtonInput::takeDeathPress` accepts one new down edge,
including simultaneous buttons, and then waits for release. A button waking STOP
when the Pet was already Dead resets after voltage recovery and RTC observation;
a wake that discovers a newly expired Alive Pet is consumed without resetting.
Boot-held keys are consumed too. ISR handling remains wake-only.

Implementation was reviewed statically. Tests, target builds, resource size
measurements and device acceptance were deferred at the user's request. Device
acceptance remains necessary for SD failures, low-voltage recovery, multi-key
edges, STOP wake, animation looping and restored saves.
