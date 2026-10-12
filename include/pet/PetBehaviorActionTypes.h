#ifndef PET_BEHAVIOR_ACTION_TYPES_H
#define PET_BEHAVIOR_ACTION_TYPES_H

#include <stdint.h>
#include "resources/AssetRuntimeContract.h"
#include "common/AppProfile.h"
#include "pet/RuntimeValueResolver.h"

constexpr uint8_t kMaxPetBehaviorActions = 8;
constexpr uint8_t kMaxPetBehaviorFollowingAnimationsPerAction = 3;
constexpr uint8_t kMaxPetBehaviorFollowingAnimations =
    kMaxPetBehaviorActions * kMaxPetBehaviorFollowingAnimationsPerAction;
constexpr uint8_t kMaxPetBehaviorActionConditionsPerAction = 4;
constexpr uint8_t kMaxPetBehaviorActionConditions =
    kMaxPetBehaviorActions * kMaxPetBehaviorActionConditionsPerAction;
constexpr uint8_t kMinPetBehaviorRandomOutcomesPerAction = 2;
constexpr uint8_t kMaxPetBehaviorRandomOutcomesPerAction = 3;
constexpr uint8_t kMaxPetBehaviorActionOutcomes =
    kMaxPetBehaviorActions * kMaxPetBehaviorRandomOutcomesPerAction;
constexpr uint16_t kMaxPetBehaviorActionEffects =
    kMaxPetBehaviorActionOutcomes * APP_MAX_PET_STATS;
static_assert(kMaxPetBehaviorActionEffects <= UINT16_MAX,
              "Action effect ranges require two-byte indices.");

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
    uint8_t firstFollowingAnimation;
    uint8_t followingAnimationCount;
};

struct PetBehaviorActionOutcomeConfig
{
    PetBehaviorAnimationPlaybackConfig animationPlayback;
    uint8_t weight;
    uint16_t firstEffect;
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
