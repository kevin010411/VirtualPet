#ifndef PET_STATE_CLASSIFIER_H
#define PET_STATE_CLASSIFIER_H

#include <stdint.h>

#include "pet/domain/Pet.h"
#include "pet_behavior/domain/PetBehaviorTypes.h"

constexpr uint8_t kDefaultPetStateSlot = UINT8_MAX;

struct ActivePetState
{
    uint8_t slot = kDefaultPetStateSlot;
    AssetData::AnimationRef idleAnimation = {};
    bool isDefault = true;
};

class PetStateClassifier
{
public:
    static ActivePetState classify(const PetBehaviorConfig &config,
                                   const PetStatSnapshot &snapshot);
};

#endif // PET_STATE_CLASSIFIER_H
