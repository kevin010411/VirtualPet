#ifndef PET_BEHAVIOR_RUNTIME_RULES_H
#define PET_BEHAVIOR_RUNTIME_RULES_H

#include "pet/PetBehaviorTypes.h"

struct PetBehaviorStatValues
{
    uint32_t values[kPetBehaviorSlotCount];
    uint32_t stageDays;
};

struct PetBehaviorDailyChangePauses
{
    uint8_t remainingDays[kPetBehaviorSlotCount];
};

using PetBehaviorActionPlayback = PetBehaviorAnimationPlaybackConfig;
using PetBehaviorEffectReader = bool (*)(void *, uint16_t, PetBehaviorActionEffectConfig &);

using PetBehaviorRandomBoundedSource = uint16_t (*)(uint16_t upperExclusive);

void initializePetBehaviorStats(const PetBehaviorConfig &config, PetBehaviorStatValues &state);
void applyPetBehaviorDailyChanges(const PetBehaviorConfig &config,
                                  PetBehaviorStatValues &state,
                                  PetBehaviorDailyChangePauses &pauses, uint32_t days = 1);
uint32_t petBehaviorRangeChangeDays(const PetBehaviorConfig &config,
                                   const PetBehaviorStatValues &state,
                                   const PetBehaviorDailyChangePauses &pauses,
                                   const RuntimeRangePredicate &predicate, uint32_t limit);
uint32_t petBehaviorCatchUpDays(const PetBehaviorConfig &config,
                              const PetBehaviorStatValues &state,
                              const PetBehaviorDailyChangePauses &pauses, uint32_t limit);
bool applyPetBehaviorAction(const PetBehaviorConfig &config,
                            uint8_t actionSlot,
                            PetBehaviorStatValues &state,
                            PetBehaviorDailyChangePauses &pauses,
                            PetBehaviorActionPlayback &playback,
                            PetBehaviorRandomBoundedSource randomSource = nullptr,
                            PetBehaviorEffectReader effectReader = nullptr, void *effectContext = nullptr);
#if ENABLE_GUESS_GAME
bool applyPetBehaviorGuessOutcome(const PetBehaviorConfig &config,
                                  PetBehaviorGuessOutcome outcome,
                                  PetBehaviorStatValues &state);
#endif
#if ENABLE_INSTRUCTION_RESPONSE
bool applyPetBehaviorInstructionResults(const PetBehaviorConfig &config,
                                        uint32_t correctMask, uint8_t count,
                                        PetBehaviorStatValues &state);
#endif
#endif // PET_BEHAVIOR_RUNTIME_RULES_H
