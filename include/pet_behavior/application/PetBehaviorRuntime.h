#ifndef PET_BEHAVIOR_RUNTIME_H
#define PET_BEHAVIOR_RUNTIME_H

#include <stdint.h>

#include "pet_behavior/domain/PetBehaviorRuntimeRules.h"

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
    bool advancePetDay();
    bool executeAction(uint8_t actionSlot);
#if ENABLE_GUESS_GAME
    bool applyGuessOutcome(PetBehaviorGuessOutcome outcome);
#endif
    const char *baseAnimation() const;

private:
    const PetBehaviorConfig &config;
    PetActionController &petActions;
    AnimationController &animations;
    PetBehaviorDailyChangePauses dailyChangePauses;
};

#endif // PET_BEHAVIOR_RUNTIME_H
