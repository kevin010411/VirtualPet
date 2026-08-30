#ifndef RUNTIME_CONTRACT_LOADER_H
#define RUNTIME_CONTRACT_LOADER_H

#include <SdFat.h>
#include "pet_behavior/domain/PetBehaviorTypes.h"

// Loads the complete production runtime model from /runtime.bin. The caller's
// configuration is published only after every binary-owned feature validates.
bool loadRuntimeContract(SdFat *sd,
                         uint8_t speciesSlot,
                         uint8_t outfitSlot,
                         PetBehaviorConfig &config,
                         char *errorResource = nullptr,
                         size_t errorResourceCapacity = 0);

#endif // RUNTIME_CONTRACT_LOADER_H
