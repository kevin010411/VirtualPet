# Profile feature flags

`platformio.ini` is the single source of truth for each firmware profile.  Every
feature flag has a backwards-compatible default in
`include/shared/config/AppProfile.h`; a new profile must set its own value
explicitly when it wants the smallest binary.

## Flags already compiled out

| Flag | What it removes | Keep enabled when |
| --- | --- | --- |
| `ENABLE_GUESS_ITEM_GAME` | Guess-item game, input flow and its command | The `HAVE_FUN` command is offered. |
| `ENABLE_STARTUP_ANIMATION` | Startup animation path | A `START` animation is part of the profile. |
| `ENABLE_FIRST_LAUNCH_SELECTION` | First-launch selection flow | First boot must force an appearance choice. |
| `ENABLE_OUTFIT_CHOOSE_ANIMATION` | Outfit confirmation animation | Outfit selection needs its preview animation. |
| `ENABLE_DYNAMIC_ACTION_LAYOUT` | SD-driven action-layout parser | The profile supplies dynamic action layouts. |
| `ENABLE_RENDER_STATS` | Render timing/statistics instrumentation | Rendering performance diagnostics are required. |
| `ENABLE_APPEARANCE_SELECTION` | Appearance-selection controller and its rendering/input flow | The profile exposes change outfit/species, or first launch selects appearance. |
| `ENABLE_COMMAND_PREDICT` | Prediction command and its 11-result animation dispatch | `PREDICT` is in the profile menu. |
| `ENABLE_COMMAND_GIFT` | Visible gift command | `GIFT` is in the profile menu. This does not remove the guess-game fallback animation. |
| `ENABLE_COMMAND_OUTFIT` | Visible change-outfit command | `CHANGE_OUTFIT` is in the profile menu. |
| `ENABLE_COMMAND_SPECIES` | Visible change-species command | `CHANGE_SPECIES` is in the profile menu. |
| `APP_STATUS_MODE` | All but one status-display implementation | Select exactly the status implementation used by the profile. |

`ENABLE_GUESS_GAME_SINGLE_ROUND` changes the number of game rounds; it does
not remove the minigame implementation, so it is not a Flash-saving gate.

`ENABLE_SD_RLE_ASSETS` was removed because no source-level conditional used it.

`ENABLE_APPEARANCE_SELECTION=0` is rejected at compile time when an appearance
command or first-launch selection is enabled. This prevents an invalid profile
configuration from producing a binary that cannot complete its flow.

## Current profile matrix

`1` means compiled in; `0` means excluded. `small_start` inherits the `small`
command/appearance flags and only enables startup animation. `stm32` inherits
`default`.

| Profile | Guess | Start | First launch | Dynamic layout | Appearance selection | Predict | Gift | Outfit cmd | Species cmd | Status |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `default` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | 0 | 0 | single meter |
| `new_taipei_childrens_day` | 0 | 1 | 1 | 0 | 1 | 0 | 1 | 0 | 0 | direct |
| `kuromu` | 0 | 0 | 0 | 1 | 1 | 0 | 1 | 1 | 0 | composite health |
| `small_multi_status` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 1 | random meters |
| `small_status_anime` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 1 | direct |
| `dipsyho` | 1 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | triple meter |
| `small` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 1 | single meter |
| `small_start` | 1 | 1 | 0 | 0 | 1 | 0 | 0 | 0 | 1 | single meter |
| `stm32` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | 0 | 0 | single meter |

## Deliberately not disabled yet

These are candidates for additional flags, but their need cannot be inferred
from the compiled menu alone:

| Candidate | Why it is not automatically disabled |
| --- | --- |
| `CustomRules` | Only Kuromu currently exposes `CUSTOM0`, but `/custom_rules.txt` can also apply daily stats and gift variant effects without a custom menu item. Define which profiles may use those SD rules before introducing `ENABLE_CUSTOM_RULES=0`. |
| Evolution | Evolution targets are read from the appearance data and can run automatically in every profile. |
| SD appearance loader / asset manifest | Initial pet state, evolution and animation rendering all depend on these. |
| Low-battery protection (PVD) | It is tied to the shared STM32 hardware, not a visual profile. Disabling it changes power-loss behavior. |
| Sleep, buzzer, TFT-repair long press, and reset-pet combo | These are device-level controls shared by all profiles; removal needs a hardware/product decision. |

If a future profile does not need one of the last group, add the flag in
`AppProfile.h`, set it explicitly in that environment, and build every affected
environment before merging.
