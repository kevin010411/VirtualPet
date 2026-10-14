#ifndef PET_BEHAVIOR_RUNTIME_H
#define PET_BEHAVIOR_RUNTIME_H

#include <stdint.h>

#include "pet/PetBehaviorRuntimeRules.h"
#include "pet/PetStateClassifier.h"

class AnimationController;
class Pet;
class Renderer;
class AppearanceLoader;

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
                       Pet &pet,
                       AnimationController &animations,
                       Renderer &renderer);

    bool hasAction(uint8_t actionSlot) const;
    void initializeStats();
    void resetTransientState() { dailyChangePauses = {}; }
    bool isDead() const;
    bool advancePetDay(uint32_t days = 1);
    bool catchUpDays(AppearanceLoader &loader, uint32_t limit, uint32_t &days) const;
    PetBehaviorActionResult executeAction(uint8_t actionSlot);
#if ENABLE_GUESS_GAME
    bool applyGuessOutcome(PetBehaviorGuessOutcome outcome);
#endif
    AssetData::AnimationRef baseAnimation() const;
#if ENABLE_SEQUENCE_AUDIO
    SequenceAudioRef baseAudio() const;
#endif
    ActivePetState activePetState() const;
    ActivePetState activePetState(const PetStatSnapshot &snapshot) const;

private:
    const PetBehaviorConfig &config;
    Pet &pet;
    AnimationController &animations;
    Renderer &renderer;
    PetBehaviorDailyChangePauses dailyChangePauses;
};

#endif // PET_BEHAVIOR_RUNTIME_H
