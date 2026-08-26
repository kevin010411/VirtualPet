#ifndef PET_BEHAVIOR_RUNTIME_RULES_H
#define PET_BEHAVIOR_RUNTIME_RULES_H

#include "pet_behavior/domain/PetBehaviorTypes.h"

struct PetBehaviorStatValues
{
    int16_t values[kPetBehaviorSlotCount];
    uint32_t stageDays;
};

struct PetBehaviorDailyChangePauses
{
    uint8_t remainingDays[kPetBehaviorSlotCount];
};

struct PetBehaviorActionPlayback
{
    char animation[kPetBehaviorAnimationTokenSize];
    uint8_t playbackCount;
};

void initializePetBehaviorStats(const PetBehaviorConfig &config, PetBehaviorStatValues &state);
void applyPetBehaviorDailyChanges(const PetBehaviorConfig &config,
                                  PetBehaviorStatValues &state,
                                  PetBehaviorDailyChangePauses &pauses);
bool applyPetBehaviorAction(const PetBehaviorConfig &config,
                            uint8_t actionSlot,
                            PetBehaviorStatValues &state,
                            PetBehaviorDailyChangePauses &pauses,
                            PetBehaviorActionPlayback &playback);
#if ENABLE_GUESS_GAME
bool applyPetBehaviorGuessOutcome(const PetBehaviorConfig &config,
                                  PetBehaviorGuessOutcome outcome,
                                  PetBehaviorStatValues &state);
#endif
const char *resolvePetBehaviorBaseAnimation(const PetBehaviorConfig &config,
                                            const PetBehaviorStatValues &state);

#endif // PET_BEHAVIOR_RUNTIME_RULES_H
