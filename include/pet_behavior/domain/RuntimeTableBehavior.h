#ifndef RUNTIME_TABLE_BEHAVIOR_H
#define RUNTIME_TABLE_BEHAVIOR_H

#include <SdFat.h>
#include <stddef.h>
#include <stdint.h>
#include "pet_behavior/domain/PetBehaviorTypes.h"

class BundleReader;
struct AppearanceSelection;

// Reads the bounded /runtime.bin envelope before any asset pack is configured.
// Export and the host inspector own complete catalog and integrity validation.
bool loadRuntimeManifest(SdFat *sd, AssetData::RuntimeManifest &manifest);

// Loads execution-owned records from one validated /runtime.bin snapshot into
// caller-owned storage. Failure invalidates config and is fatal to the session;
// initialAppearanceResolved identifies whether initial appearance decoding passed.
bool loadCompleteRuntimeTable(SdFat *sd,
                              const AssetData::RuntimeManifest &manifest,
                              BundleReader &bundleReader,
                              uint8_t speciesSlot,
                              uint8_t outfitSlot,
                              PetBehaviorConfig &config,
                              AppearanceSelection *initialAppearance = nullptr,
                              bool *initialAppearanceResolved = nullptr);

// Decodes behavior and presentation records from a complete Runtime Table v9 file.
// The supplied configuration is published only after bounded reads and runtime
// array/reference guards succeed; export and host tooling validate semantics.
bool parseRuntimeTableBehavior(const uint8_t *bytes,
                               size_t byteCount,
                               const AssetData::RuntimeManifest &manifest,
                               uint8_t speciesSlot,
                               uint8_t outfitSlot,
                               PetBehaviorConfig &config);

#endif // RUNTIME_TABLE_BEHAVIOR_H
