#ifndef PET_BEHAVIOR_CONTRACT_H
#define PET_BEHAVIOR_CONTRACT_H

#include <SdFat.h>
#include "pet_behavior/domain/PetBehaviorTypes.h"

// The parser validates the runtime-contract envelope and bounded record fields before assigning config.
bool parsePetBehaviorContract(const char *contractText,
                              const AssetData::RuntimeManifest &manifest,
                              uint8_t speciesSlot,
                              uint8_t outfitSlot,
                              PetBehaviorConfig &config);

// The SD loader reads /runtime_contract.txt through the shared bounded reader and retains no source text.
bool loadPetBehaviorContract(SdFat *sd,
                             uint8_t speciesSlot,
                             uint8_t outfitSlot,
                             PetBehaviorConfig &config,
                             char *errorResource = nullptr,
                             size_t errorResourceCapacity = 0);

#endif // PET_BEHAVIOR_CONTRACT_H
