#ifndef PET_BEHAVIOR_TYPES_H
#define PET_BEHAVIOR_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include "animation/domain/Animation.h"
#include "commands/domain/StatusSetContract.h"
#include "commands/domain/SystemCommandCatalog.h"
#include "pet_behavior/domain/RuntimeValueResolver.h"
#include "shared/config/AppProfile.h"

constexpr uint8_t kPetBehaviorSlotCount = APP_MAX_PET_STATS;
constexpr uint8_t kMaxPetBehaviorStats = kPetBehaviorSlotCount;
static_assert(kPetBehaviorSlotCount > 0, "Pet Stat capacity must be positive.");
static_assert(kPetBehaviorSlotCount <= 10, "Pet Stat Slot tokens require one decimal digit.");
constexpr uint8_t kMaxRuntimePetStates = 16;
constexpr uint8_t kMaxPetBehaviorActions = 8;
constexpr uint8_t kMaxPetBehaviorActionConditionsPerAction = 4;
constexpr uint8_t kMaxPetBehaviorActionConditions =
    kMaxPetBehaviorActions * kMaxPetBehaviorActionConditionsPerAction;
constexpr uint8_t kMinPetBehaviorRandomOutcomesPerAction = 2;
constexpr uint8_t kMaxPetBehaviorRandomOutcomesPerAction = 3;
constexpr uint8_t kMaxPetBehaviorActionEffects = kMaxPetBehaviorActions * kMaxPetBehaviorStats;
constexpr uint16_t kMaxPetBehaviorRandomOutcomeEffects =
    kMaxPetBehaviorActions * kMaxPetBehaviorRandomOutcomesPerAction * kMaxPetBehaviorStats;
static_assert(kMaxPetBehaviorRandomOutcomeEffects <= UINT8_MAX,
              "Random Outcome effect tokens require one byte.");
#if ENABLE_GUESS_GAME
constexpr uint8_t kPetBehaviorGuessOutcomeCount = 4;
constexpr uint8_t kMaxPetBehaviorGuessEffects = kPetBehaviorGuessOutcomeCount * kMaxPetBehaviorStats;
#endif
constexpr uint8_t kPetBehaviorButtonCount = 8;

enum class PetBehaviorEffectOperation : uint8_t
{
    Change,
    Set,
};

struct PetBehaviorStatConfig
{
    bool active;
    int16_t initialValue;
    int16_t minValue;
    int16_t maxValue;
    int16_t dailyChange;
};

struct RuntimePetStateConfig
{
    RuntimeRangePredicate predicate;
    AssetData::AnimationRef idleAnimation;
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

struct PetBehaviorActionConfig
{
    bool active;
    PetBehaviorActionMode mode;
    bool hasFallbackAnimation;
    PetBehaviorAnimationPlaybackConfig animationPlayback;
    uint8_t suspendDailyChangeDays;
};

struct PetBehaviorRandomOutcomeConfig
{
    bool active;
    uint8_t weight;
    PetBehaviorAnimationPlaybackConfig animationPlayback;
};

struct PetBehaviorActionConditionConfig
{
    bool active;
    uint8_t actionSlot;
    uint8_t priority;
    RuntimeRangePredicate predicate;
    PetBehaviorAnimationPlaybackConfig animationPlayback;
};

struct PetBehaviorActionEffectConfig
{
    using Operation = PetBehaviorEffectOperation;

    bool active;
    uint8_t actionSlot;
    uint8_t statSlot;
    Operation operation;
    int16_t value;
};

struct PetBehaviorRandomOutcomeEffectConfig
{
    bool active;
    uint8_t actionSlot;
    uint8_t outcomeSlot;
    uint8_t statSlot;
    PetBehaviorEffectOperation operation;
    int16_t value;
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
    int16_t value;
};
#endif

enum class PetBehaviorButtonKind : uint8_t
{
    Empty,
    UserAction,
    SystemCommand,
};

struct PetBehaviorButtonConfig
{
    bool active;
    PetBehaviorButtonKind kind;
    uint8_t actionSlot;
    RuntimeSystemCommandId systemCommandId;
};

constexpr uint8_t kMaxScreenBlocks = 32;
constexpr uint8_t kMaxScreenRules = 64;
constexpr uint8_t kUnboundScreenSource = 255;

enum class ScreenBlockKind : uint8_t
{
    Animation = 1,
    Button = 2,
    Stat = 3,
};

struct ScreenBlockConfig
{
    ScreenBlockKind kind;
    uint8_t source;
    uint8_t x;
    uint8_t y;
    uint8_t width;
    uint8_t height;
    uint8_t firstRule;
    uint8_t ruleCount;
    uint16_t fallbackFrame;
};

struct ScreenRuleConfig
{
    int32_t minimum;
    int32_t maximum;
    uint16_t frame;
};

struct PetBehaviorConfig
{
    AssetData::RuntimeManifest assetManifest;
    AssetData::AnimationRef systemAnimations[kFirmwarePlaybackRoleCount];
    AssetData::AnimationRef layoutUnselected;
    AssetData::AnimationRef layoutSelected;
    uint32_t schemaFingerprint;
    PetBehaviorStatConfig stats[kPetBehaviorSlotCount];
    RuntimePetStateConfig petStates[kMaxRuntimePetStates];
    PetBehaviorActionConfig actions[kMaxPetBehaviorActions];
    PetBehaviorRandomOutcomeConfig
        randomOutcomes[kMaxPetBehaviorActions][kMaxPetBehaviorRandomOutcomesPerAction];
    PetBehaviorActionConditionConfig actionConditions[kMaxPetBehaviorActionConditions];
    PetBehaviorActionEffectConfig actionEffects[kMaxPetBehaviorActionEffects];
    PetBehaviorRandomOutcomeEffectConfig randomOutcomeEffects[kMaxPetBehaviorRandomOutcomeEffects];
#if ENABLE_GUESS_GAME
    PetBehaviorGuessEffectConfig guessEffects[kMaxPetBehaviorGuessEffects];
#endif
    PetBehaviorButtonConfig buttons[kPetBehaviorButtonCount];
    ScreenBlockConfig screenBlocks[kMaxScreenBlocks];
    uint8_t screenBlockCount;
    ScreenRuleConfig screenRules[kMaxScreenRules];
    uint8_t screenRuleCount;
    uint16_t screenProductFrameCount;
    StatusSetsConfig statusSets;
    AssetData::AnimationRef idleAnimation;
    uint8_t activeSpeciesSlot;
    uint8_t activeOutfitSlot;
    uint8_t statCount;
    uint8_t petStateCount;
    uint8_t actionCount;
    uint8_t actionConditionCount;
    uint8_t actionEffectCount;
    uint16_t randomOutcomeEffectCount;
#if ENABLE_GUESS_GAME
    uint8_t guessEffectCount;
#endif
    uint8_t buttonCount;
};

#endif // PET_BEHAVIOR_TYPES_H
