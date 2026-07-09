# Architecture

This project uses a feature-first, lightweight Clean Architecture layout. Files
are grouped by the feature they serve first, then by architectural role inside
that feature.

## Module Layout

- `pet/`: pet state, growth rules, and pet persistence use cases.
- `appearance/`: species and outfit selection, appearance lookup ports, and SD
  backed appearance adapters.
- `animation/`: animation ids, animation metadata, and animation scheduling.
- `commands/`: command menu state and command execution.
- `minigames/guess_item/`: guess-item game rules and its application controller.
- `presentation/`: app flow and rendering-facing UI orchestration.
- `platform/`: board, display, button, SD, and MCU integration code.
- `shared/`: cross-feature configuration and asset metadata.

## Layers

Feature folders use these layer names when they apply:

- `domain`: pure state and rules with no hardware ownership.
- `application`: use cases and controllers that coordinate domain behavior.
- `ports`: interfaces required by application code.
- `adapters`: concrete hardware, SD, rendering, or framework implementations.

Dependencies should point inward where practical: application code may depend on
domain and ports, while adapters implement ports or provide concrete platform
services. This is intentionally lightweight; existing behavior and embedded
resource constraints matter more than introducing abstractions for their own
sake.

## Placement Rules

- Put new pet rules in `pet/domain` and pet workflows in `pet/application`.
- Put SD-backed or hardware-backed implementations in `adapters` or `platform`.
- Put rendering pipeline code under `presentation/adapters/rendering` unless it
  is shared asset metadata, which belongs in `shared/assets`.
- Put compile-time feature flags and profile constants in `shared/config`.
- Do not add compatibility headers for old paths such as `app/`, `domain/`,
  `render/`, `storage/`, `hardware/`, or `minigame/`; update includes to the
  feature-first path instead.

## Verification

After structural changes, build at least the default target environment:

```sh
platformio run -e kuromu
```

For feature-flag-sensitive changes, also build representative profiles such as
`default`, `small`, `small_multi_status`, and `new_taipei_childrens_day`.
