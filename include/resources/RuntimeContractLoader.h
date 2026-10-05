#ifndef RUNTIME_CONTRACT_LOADER_H
#define RUNTIME_CONTRACT_LOADER_H

#include <SdFat.h>
#include "pet/PetBehaviorTypes.h"

struct AppearanceSelection;

// Loads the complete production runtime model from /runtime.bin. A failed load
// leaves the caller's configuration cleared; recovery requires a reboot.
// Ordinary appearance changes reload the manifest before the complete table.
bool loadRuntimeContract(SdFat *sd,
                         uint8_t speciesSlot,
                         uint8_t outfitSlot,
                         PetBehaviorConfig &config,
                         char *errorResource = nullptr,
                         size_t errorResourceCapacity = 0);

// Reads the startup manifest, then resolves the initial appearance and loads
// its complete contract from one further /runtime.bin open.
// initialAppearanceResolved distinguishes lookup failures from later behavior
// or presentation failures.
bool loadInitialRuntimeContract(SdFat *sd,
                                AppearanceSelection &selection,
                                PetBehaviorConfig &config,
                                bool &initialAppearanceResolved,
                                char *errorResource = nullptr,
                                size_t errorResourceCapacity = 0);

#endif // RUNTIME_CONTRACT_LOADER_H
