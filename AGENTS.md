# Cross-repository specification

Before designing or changing Pet Stats, Pet Behavior, User Actions, Status,
Evolution conditions, System Commands, pet persistence, SD initialization, or
firmware profiles, read the authoritative cross-layer specification at:

`E:\C++\virtualPet\web\.scratch\sd-driven-pet-stats\spec.md`

Do not duplicate that specification in this firmware repository. Keep firmware
documentation limited to firmware-specific implementation and verification.

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
