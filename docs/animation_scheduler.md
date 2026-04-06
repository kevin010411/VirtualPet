# Animation Scheduler Rules

The scheduler decides animation playback order using `owner` and `priority`.

## Owners

- `BaseState`
  - Pet idle/status animations such as `Idle`, `Hungry`, `Sick`
  - These should be treated as fallback visuals
- `Command`
  - Button-triggered actions such as `Feed`, `Clean`, `Gift`, `Predict`
  - These represent user intent and should override base state playback
- `Minigame`
  - Interactive game flow such as apple prompts, win/loss, result feedback
  - These must not be interrupted by base-state playback
- `System`
  - Reserved for boot, error, recovery, or future critical animations

## Priorities

- `Base`
  - Default priority for passive state animations
- `Normal`
  - Non-critical command or status-card style animations
- `High`
  - Standard user-triggered action animations
- `Critical`
  - Minigame and hard-lock sequences that must finish before fallback resumes

## Current playback policy

- A currently playing non-base animation stays active until it finishes
- Base-state animation is only shown when no owned animation is active
- Pending animations are sorted by priority before playback
- For equal priority, queue order is preserved
- Clearing animations by owner removes pending items from that owner and can also stop the currently active animation if it belongs to that owner

## Default mapping used by firmware

- `BaseState` + `Base`
  - Pet status animations
- `Command` + `High`
  - Feed, clean, medicine, shower, gift, predict pipeline
- `Command` + `Normal`
  - Status card / informational animations
- `Minigame` + `Critical`
  - Guess-apple prompts, result panels, win/loss animations

## Why this exists

This policy prevents:

- Pet status animations from stealing the screen during minigames
- User action animations from being replaced by passive idle updates
- Mid-sequence command animations from being reordered by lower-priority events

## Planned manifest integration

In the current firmware, owner and priority are assigned in code.

Long term, `index.txt` may also include playback hints such as:

```txt
id=GuessWin;path=/assets/minigame/guess/win;format=raw_sequence;frames=4;width=128;height=96;fps=10;owner=Minigame;priority=Critical
```

That should be treated as metadata for scheduler defaults, not as a replacement for runtime game logic.
