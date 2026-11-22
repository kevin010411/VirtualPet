# Cross-repository specification

Before changing Pet Stats, Pet Behavior, User Actions, Status, Evolution, System Commands,
pet persistence, animation audio, SD loading, or firmware profiles, follow the
[Web–firmware contract entry](docs/README.md#跨端規格). Keep shared contracts in
the Web repository and firmware implementation notes in this repository.

For local builds and verification, use the [development workflow](docs/development-workflow.md)
and [host test index](test/README.md).

# Validation ownership

Data and asset validation belongs to the Web backend/exporter. Firmware consumes
the validated exported contract; keeping firmware Flash small is a requirement.

Do not add firmware-side validation of exported data or assets, including runtime
table structure, references, collection conditions, activation targets, image
dimensions, decode-only asset scans, or whole-file CRC scans. Implement missing
validation in the backend/exporter before it emits the SD package, rather than
duplicating that validation in firmware.

Firmware handles actual SD read/write and hardware execution failures. This
documentation change does not authorize removing existing validation code.
