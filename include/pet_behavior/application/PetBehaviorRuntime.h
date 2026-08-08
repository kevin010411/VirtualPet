#ifndef PET_BEHAVIOR_RUNTIME_H
#define PET_BEHAVIOR_RUNTIME_H

#include <stdint.h>

#include "pet_behavior/domain/PetBehaviorContract.h"

class AnimationController;
class PetActionController;

class PetBehaviorRuntime
{
public:
    PetBehaviorRuntime(const PetBehaviorConfig &config,
                       PetActionController &petActions,
                       AnimationController &animations);

    bool hasAction(uint8_t actionSlot) const;
    void initializeStats();
    void applyDailyChanges();
    bool executeAction(uint8_t actionSlot);
    const char *baseAnimation() const;

private:
    const PetBehaviorConfig &config;
    PetActionController &petActions;
    AnimationController &animations;
};

#endif // PET_BEHAVIOR_RUNTIME_H
