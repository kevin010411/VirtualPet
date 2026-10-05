#ifndef PET_BEHAVIOR_ACTION_TYPES_H
#define PET_BEHAVIOR_ACTION_TYPES_H

#include <stdint.h>
#include "shared/assets/AssetRuntimeContract.h"
#include "shared/config/AppProfile.h"
#include "pet_behavior/domain/RuntimeValueResolver.h"

constexpr uint8_t kMaxPetBehaviorActions = 8;
constexpr uint8_t kMaxPetBehaviorActionConditionsPerAction = 4;
constexpr uint8_t kMaxPetBehaviorActionConditions =
    kMaxPetBehaviorActions * kMaxPetBehaviorActionConditionsPerAction;
constexpr uint8_t kMinPetBehaviorRandomOutcomesPerAction = 2;
constexpr uint8_t kMaxPetBehaviorRandomOutcomesPerAction = 3;
constexpr uint8_t kMaxPetBehaviorActionOutcomes =
    kMaxPetBehaviorActions * kMaxPetBehaviorRandomOutcomesPerAction;
constexpr uint16_t kMaxPetBehaviorActionEffects =
    kMaxPetBehaviorActionOutcomes * APP_MAX_PET_STATS;
static_assert(kMaxPetBehaviorActionEffects <= UINT8_MAX,
              "Action effect ranges require one-byte indices.");

enum class PetBehaviorEffectOperation : uint8_t
{
    Change,
    Set,
};

enum class PetBehaviorActionMode : uint8_t
{
    Standard,
    ConditionalAnimation,
    RandomOutcome,
};

struct PetBehaviorAnimationPlaybackConfig
{
    AssetData::AnimationRef animation;
    uint8_t playbackCount;
};

// Ranges own contiguous records in PetBehaviorConfig. Standard/conditional
// Actions have one Outcome; random Actions have two or three. Child records
// need neither occupancy flags nor a repeated parent Action/Outcome identity.
struct PetBehaviorActionConfig
{
    bool active;
    PetBehaviorActionMode mode;
    bool hasFallbackAnimation;
    uint8_t suspendDailyChangeDays;
    uint8_t firstOutcome;
    uint8_t outcomeCount;
    uint8_t firstCondition;
    uint8_t conditionCount;
};

struct PetBehaviorActionOutcomeConfig
{
    PetBehaviorAnimationPlaybackConfig animationPlayback;
    uint8_t weight;
    uint8_t firstEffect;
    uint8_t effectCount;
};

struct PetBehaviorActionConditionConfig
{
    RuntimeRangePredicate predicate;
    PetBehaviorAnimationPlaybackConfig animationPlayback;
    uint8_t priority;
};

struct PetBehaviorActionEffectConfig
{
    uint8_t statSlot;
    PetBehaviorEffectOperation operation;
    int16_t value;
};

#endif // PET_BEHAVIOR_ACTION_TYPES_H
