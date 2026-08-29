#ifndef PET_BEHAVIOR_RUNTIME_H
#define PET_BEHAVIOR_RUNTIME_H

#include <stdint.h>

#include "pet_behavior/domain/PetBehaviorRuntimeRules.h"

class AnimationController;
class PetActionController;
class Renderer;

enum class PetBehaviorActionResult : uint8_t
{
    Rejected,
    Applied,
    AppliedAnimationMissing,
};

class PetBehaviorRuntime
{
public:
    PetBehaviorRuntime(const PetBehaviorConfig &config,
                       PetActionController &petActions,
                       AnimationController &animations,
                       Renderer &renderer);

    bool hasAction(uint8_t actionSlot) const;
    void initializeStats();
    bool advancePetDay();
    PetBehaviorActionResult executeAction(uint8_t actionSlot);
#if ENABLE_GUESS_GAME
    bool applyGuessOutcome(PetBehaviorGuessOutcome outcome);
#endif
    AssetData::AnimationRef baseAnimation() const;

private:
    const PetBehaviorConfig &config;
    PetActionController &petActions;
    AnimationController &animations;
    Renderer &renderer;
    PetBehaviorDailyChangePauses dailyChangePauses;
};

#endif // PET_BEHAVIOR_RUNTIME_H
