#ifndef PET_BEHAVIOR_CONTRACT_H
#define PET_BEHAVIOR_CONTRACT_H

#include <SdFat.h>
#include "pet_behavior/domain/PetBehaviorTypes.h"

// The parser validates the complete v1 contract before assigning config.
bool parsePetBehaviorContract(const char *contractText, PetBehaviorConfig &config);

// The SD loader streams the contract through the same parser and retains no source text.
bool loadPetBehaviorContract(SdFat *sd, PetBehaviorConfig &config);

#endif // PET_BEHAVIOR_CONTRACT_H
