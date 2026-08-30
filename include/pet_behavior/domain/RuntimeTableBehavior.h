#ifndef RUNTIME_TABLE_BEHAVIOR_H
#define RUNTIME_TABLE_BEHAVIOR_H

#include <SdFat.h>
#include <stddef.h>
#include <stdint.h>
#include "pet_behavior/domain/PetBehaviorTypes.h"

class BundleReader;

// Verifies the self-describing /runtime.bin envelope before any asset pack is
// configured. The consuming decode performs the remaining catalog checks.
bool loadRuntimeManifest(SdFat *sd, AssetData::RuntimeManifest &manifest);

// Loads every production-owned section from one open /runtime.bin snapshot and
// publishes the candidate only after behavior, flow, and initial appearance
// have all validated.
bool loadCompleteRuntimeTable(SdFat *sd,
                              const AssetData::RuntimeManifest &manifest,
                              BundleReader &bundleReader,
                              uint8_t speciesSlot,
                              uint8_t outfitSlot,
                              PetBehaviorConfig &config);

// Decodes the Ticket 03-owned records from a complete runtime-table v1 file.
// The supplied configuration is published only after the envelope, CRC,
// capacities, references, and behavior records all validate.
bool parseRuntimeTableBehavior(const uint8_t *bytes,
                               size_t byteCount,
                               const AssetData::RuntimeManifest &manifest,
                               uint8_t speciesSlot,
                               uint8_t outfitSlot,
                               PetBehaviorConfig &config);

#endif // RUNTIME_TABLE_BEHAVIOR_H
