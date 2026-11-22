#ifndef PET_BEHAVIOR_TYPES_H
#define PET_BEHAVIOR_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include "animation/Animation.h"
#include "pet/PetBehaviorActionTypes.h"
#include "pet/RuntimeValueResolver.h"
#include "common/AppProfile.h"

constexpr uint8_t kPetBehaviorSlotCount = APP_MAX_PET_STATS;
constexpr uint8_t kMaxPetBehaviorStats = kPetBehaviorSlotCount;
static_assert(kPetBehaviorSlotCount > 0, "Pet Stat capacity must be positive.");
static_assert(kPetBehaviorSlotCount <= 12, "Pet Stat capacity must fit the runtime contract.");
constexpr uint8_t kMaxRuntimePetStates = 16;
#if ENABLE_GUESS_GAME
constexpr uint8_t kPetBehaviorGuessOutcomeCount = 4;
constexpr uint8_t kMaxPetBehaviorGuessEffects = kPetBehaviorGuessOutcomeCount * kMaxPetBehaviorStats;
#endif

struct PetBehaviorStatConfig
{
    bool active;
    uint32_t initialValue;
    uint32_t minValue;
    uint32_t maxValue;
    int64_t dailyChange;
};

struct RuntimePetStateConfig
{
    RuntimeRangePredicate predicate;
    AssetData::AnimationRef idleAnimation;
};

#if ENABLE_GUESS_GAME
enum class PetBehaviorGuessOutcome : uint8_t
{
    RoundCorrect,
    RoundWrong,
    GameWin,
    GameLoss,
};

struct PetBehaviorGuessEffectConfig
{
    bool active;
    PetBehaviorGuessOutcome outcome;
    uint8_t statSlot;
    PetBehaviorEffectOperation operation;
    int64_t value;
};
#endif

struct PetBehaviorConfig
{
    PetBehaviorActionEffectConfig dayResets[kPetBehaviorSlotCount];
    uint32_t actionGatesOffset;
    uint16_t actionGateCount;
    bool deathEnabled;
    uint16_t deathStatMask;
    AssetData::AnimationRef deathAnimation;
    uint8_t trackedActionCount;
    uint8_t trackedActions[8];
    PetBehaviorStatConfig stats[kPetBehaviorSlotCount];
    RuntimePetStateConfig petStates[kMaxRuntimePetStates];
    PetBehaviorActionConfig actions[kMaxPetBehaviorActions];
    PetBehaviorAnimationPlaybackConfig followingAnimations[kMaxPetBehaviorFollowingAnimations];
    PetBehaviorActionOutcomeConfig actionOutcomes[kMaxPetBehaviorActionOutcomes];
    PetBehaviorActionConditionConfig actionConditions[kMaxPetBehaviorActionConditions];
    PetBehaviorActionEffectConfig actionEffects[kStoredPetBehaviorActionEffects];
    uint32_t actionEffectsOffset;
#if ENABLE_GUESS_GAME
    PetBehaviorGuessEffectConfig guessEffects[kMaxPetBehaviorGuessEffects];
#endif
    AssetData::AnimationRef idleAnimation;
    uint8_t statCount;
    uint8_t petStateCount;
    uint8_t actionCount;
    uint8_t actionConditionCount;
    uint16_t actionEffectCount;
    uint8_t actionOutcomeCount;
    uint8_t dayResetCount;
    bool manualDays;
    bool actionEffectsStreamed;
#if ENABLE_GUESS_GAME
    uint8_t guessEffectCount;
#endif
};

#endif // PET_BEHAVIOR_TYPES_H
