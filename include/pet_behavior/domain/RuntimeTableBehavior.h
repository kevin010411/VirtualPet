#ifndef RUNTIME_TABLE_BEHAVIOR_H
#define RUNTIME_TABLE_BEHAVIOR_H

#include <SdFat.h>
#include <stddef.h>
#include <stdint.h>
#include "pet_behavior/domain/PetBehaviorTypes.h"

// Decodes the Ticket 03-owned records from a complete runtime-table v1 file.
// The supplied configuration is published only after the envelope, CRC,
// capacities, references, and behavior records all validate.
bool parseRuntimeTableBehavior(const uint8_t *bytes,
                               size_t byteCount,
                               const AssetData::RuntimeManifest &manifest,
                               uint8_t speciesSlot,
                               uint8_t outfitSlot,
                               PetBehaviorConfig &config);

// SD composition entry point used during the expand-contract migration.
bool loadRuntimeTableBehavior(SdFat *sd,
                              const AssetData::RuntimeManifest &manifest,
                              uint8_t speciesSlot,
                              uint8_t outfitSlot,
                              PetBehaviorConfig &config);

// Ticket 05-owned binary flow composition.  This replaces the numeric runtime
// roles and layouts populated from temporary TXT contracts during migration.
bool loadRuntimeTableFlow(SdFat *sd,
                          const AssetData::RuntimeManifest &manifest,
                          uint8_t speciesSlot,
                          uint8_t outfitSlot,
                          PetBehaviorConfig &config);

#endif // RUNTIME_TABLE_BEHAVIOR_H
