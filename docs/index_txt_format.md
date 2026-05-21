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
- Keeps `bmp`, `raw`, and `rle` resources on the same schema
- Supports reusable path templates for multiple animals that share the same asset layout

## Line format

Each non-empty line must use this fixed field order:

```txt
id|format|frames|width|height|fps|path
```

Example:

```txt
Idle|bmp|6|128|96|6|/{animal}/animation/idle
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
  - `rle`
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
  - Supports the `{animal}` token, which is replaced by the firmware-selected animal name
  - Firmware will load frames as:
  - `path/1.bmp`, `path/2.bmp`, ... for `bmp`
  - `path/1.raw`, `path/2.raw`, ... for `raw`
  - `path/1.rle`, `path/2.rle`, ... for `rle`

## Validation rules

- Every resource line must contain exactly 7 fields
- Unknown `id` values are ignored
- Unknown `format` values are ignored
- `frames`, `width`, or `height` equal to `0` are ignored
- Empty `path` values are ignored

## Example

```txt
# Pet base status
Idle|bmp|6|128|96|6|/{animal}/animation/idle
Hungry|bmp|4|128|96|6|/{animal}/animation/hungry
Depress|bmp|4|128|96|6|/{animal}/animation/depress

# Command animations
Feed|raw|5|128|96|8|/{animal}/animation/feed
Gift|rle|6|128|96|8|/{animal}/animation/gift

# Minigame animations
GuessWin|raw|4|128|96|10|/{animal}/guess_game/win
GuessLoss|rle|4|128|96|10|/{animal}/guess_game/loss
```

## Folder layout recommendation

```txt
/index.txt
/dino/animation/idle/1.bmp
/cat/animation/hungry/1.bmp
/dog/animation/feed/1.raw
/dog/guess_game/win/1.rle
/assets/ui/layout/1.bmp
```

## Multi-animal usage

If the SD card contains:

```txt
/dino/animation/idle/1.bmp
/dog/animation/idle/1.bmp
/cat/animation/idle/1.bmp
```

Then the same manifest line:

```txt
Idle|bmp|6|128|96|6|/{animal}/animation/idle
```

can be reused by changing the firmware-side selected animal name, for example:

- `dino`
- `dog`
- `cat`
