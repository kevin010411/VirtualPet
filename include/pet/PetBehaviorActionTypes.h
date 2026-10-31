#ifndef PET_BEHAVIOR_ACTION_TYPES_H
#define PET_BEHAVIOR_ACTION_TYPES_H

#include <stdint.h>
#include "resources/AssetRuntimeContract.h"
#include "common/AppProfile.h"
#include "pet/RuntimeValueResolver.h"

constexpr uint8_t kMaxPetBehaviorActions = APP_MAX_ACTIONS;
constexpr uint8_t kMaxPetBehaviorFollowingAnimationsPerAction = 3;
constexpr uint8_t kMaxPetBehaviorFollowingAnimations =
    kMaxPetBehaviorActions * kMaxPetBehaviorFollowingAnimationsPerAction;
constexpr uint8_t kMaxPetBehaviorActionConditionsPerAction = 4;
constexpr uint8_t kMaxPetBehaviorActionConditions =
    8 * kMaxPetBehaviorActionConditionsPerAction;
constexpr uint8_t kMinPetBehaviorRandomOutcomesPerAction = 2;
constexpr uint8_t kMaxPetBehaviorRandomOutcomesPerAction = 3;
constexpr uint8_t kMaxPetBehaviorLotteryOutcomesPerAction = 28;
constexpr uint8_t kMaxPetBehaviorActionOutcomes = APP_MAX_ACTION_OUTCOMES;
constexpr uint16_t kMaxPetBehaviorActionEffects = (8 * 28 + 8 * 3) * 12;
constexpr uint16_t kStoredPetBehaviorActionEffects = APP_MAX_ACTION_EFFECTS;
static_assert(kMaxPetBehaviorActions > 0 && kMaxPetBehaviorActions <= 16, "Action storage must fit the exported profile.");
static_assert(kMaxPetBehaviorActionOutcomes > 0 && kMaxPetBehaviorActionOutcomes <= 248,
              "Action outcome storage must fit the exported profile.");
static_assert(kStoredPetBehaviorActionEffects > 0 && kStoredPetBehaviorActionEffects <= kMaxPetBehaviorActionEffects,
              "Action effect cache must fit the runtime contract.");
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
    // Zero preserves ordinary random Version choice; otherwise exact index + 1.
    uint16_t versionSelection = 0;
};

// Ranges own contiguous records in PetBehaviorConfig. Standard/conditional
// Actions have one Outcome; lottery-owned random Actions support up to 28. Child records
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
    PetBehaviorAnimationPlaybackConfig prelude;
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
    int64_t value;
};

#endif // PET_BEHAVIOR_ACTION_TYPES_H
