#ifndef RUNTIME_CONTRACT_LOADER_H
#define RUNTIME_CONTRACT_LOADER_H

#include <SdFat.h>
#include "pet_behavior/domain/PetBehaviorTypes.h"

struct AppearanceSelection;

// Loads the complete production runtime model from /runtime.bin. The caller's
// configuration is published only after every binary-owned feature validates.
// validatedManifest may be supplied only from an earlier successful manifest
// read in the same startup sequence. The complete table is still checked
// against it; ordinary appearance changes reload the manifest.
bool loadRuntimeContract(SdFat *sd,
                         uint8_t speciesSlot,
                         uint8_t outfitSlot,
                         PetBehaviorConfig &config,
                         char *errorResource = nullptr,
                         size_t errorResourceCapacity = 0,
                         const AssetData::RuntimeManifest *validatedManifest = nullptr);

// Resolves the initial appearance and loads its complete contract from the
// same /runtime.bin open. initialAppearanceResolved distinguishes lookup
// failures from later behavior or presentation failures.
bool loadInitialRuntimeContract(SdFat *sd,
                                const AssetData::RuntimeManifest &manifest,
                                AppearanceSelection &selection,
                                PetBehaviorConfig &config,
                                bool &initialAppearanceResolved,
                                char *errorResource = nullptr,
                                size_t errorResourceCapacity = 0);

#endif // RUNTIME_CONTRACT_LOADER_H
