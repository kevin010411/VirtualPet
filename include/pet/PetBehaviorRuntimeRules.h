#ifndef PET_BEHAVIOR_RUNTIME_RULES_H
#define PET_BEHAVIOR_RUNTIME_RULES_H

#include "pet/PetBehaviorTypes.h"

struct PetBehaviorStatValues
{
    uint32_t values[kPetBehaviorSlotCount];
    uint32_t stageDays;
    uint8_t speciesSlot = 0;
};

struct PetBehaviorDailyChangePauses
{
    uint8_t remainingDays[kPetBehaviorSlotCount];
};

using PetBehaviorActionPlayback = PetBehaviorAnimationPlaybackConfig;
using PetBehaviorEffectReader = bool (*)(void *, uint16_t, PetBehaviorActionEffectConfig &);

using PetBehaviorRandomBoundedSource = uint16_t (*)(uint16_t upperExclusive);

constexpr uint8_t kStageDaysEffectSlot = UINT8_MAX;
uint32_t applyStageDaysEffect(uint32_t current, PetBehaviorEffectOperation operation, int64_t value);
uint32_t applyPetBehaviorStatEffect(const PetBehaviorStatConfig &stat, uint32_t current,
                                   PetBehaviorEffectOperation operation, int64_t value);
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
                            PetBehaviorEffectReader effectReader = nullptr, void *effectContext = nullptr,
                            uint8_t *selectedOutcome = nullptr);
#if ENABLE_GUESS_GAME
bool applyPetBehaviorGuessOutcome(const PetBehaviorConfig &config,
                                  PetBehaviorGuessOutcome outcome,
                                  PetBehaviorStatValues &state);
#endif
#if ENABLE_INSTRUCTION_RESPONSE
struct InstructionRuntimeSettings;
bool applyPetBehaviorInstructionResults(const PetBehaviorConfig &config,
                                        const InstructionRuntimeSettings &settings,
                                        uint32_t correctMask, uint8_t count,
                                        PetBehaviorStatValues &state);
#endif
#endif // PET_BEHAVIOR_RUNTIME_RULES_H
