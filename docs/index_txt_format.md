# `index.txt` Format Specification

`index.txt` is the animation manifest stored on the SD card. It is the single source of truth for animation resources.

## File location

Recommended path:

```txt
/index.txt
```

## Design goals

- One line per animation resource
- Very simple MCU parsing
- Human-readable and easy to edit by hand
- Supports deep folder structures without changing firmware
- Keeps `bmp` and `raw` resources on the same schema

## Line format

Each non-empty line must use this fixed field order:

```txt
id|format|frames|width|height|fps|path
```

Example:

```txt
Idle|bmp|6|128|96|6|/assets/pet/base/idle
```

Lines starting with `#` are comments. Empty lines are ignored.

## Field definitions

- `id`
  - Must match the firmware animation id name exactly
  - Example: `Idle`, `Hungry`, `Feed`, `GuessWin`
- `format`
  - Supported values:
  - `bmp`
  - `raw`
- `frames`
  - Total frame count
  - Must be greater than `0`
- `width`
  - Frame width in pixels
  - Must be greater than `0`
- `height`
  - Frame height in pixels
  - Must be greater than `0`
- `fps`
  - Animation playback hint
  - `0` means use firmware default frame interval
- `path`
  - Folder or base path of the animation asset
  - Firmware will load frames as:
  - `path/1.bmp`, `path/2.bmp`, ... for `bmp`
  - `path/1.raw`, `path/2.raw`, ... for `raw`

## Validation rules

- Every resource line must contain exactly 7 fields
- Unknown `id` values are ignored
- Unknown `format` values are ignored
- `frames`, `width`, or `height` equal to `0` are ignored
- Empty `path` values are ignored

## Example

```txt
# Pet base status
Idle|bmp|6|128|96|6|/assets/pet/base/idle
Hungry|bmp|4|128|96|6|/assets/pet/status/hungry
Depress|bmp|4|128|96|6|/assets/pet/status/depress

# Command animations
Feed|raw|5|128|96|8|/assets/actions/feed
Gift|raw|6|128|96|8|/assets/actions/gift

# Minigame animations
GuessWin|raw|4|128|96|10|/assets/minigame/guess/win
GuessLoss|raw|4|128|96|10|/assets/minigame/guess/loss
```

## Folder layout recommendation

```txt
/index.txt
/assets/pet/base/idle/1.bmp
/assets/pet/status/hungry/1.bmp
/assets/actions/feed/1.raw
/assets/minigame/guess/win/1.raw
/assets/ui/layout/1.bmp
```
