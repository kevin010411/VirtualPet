#ifndef PET_BEHAVIOR_TYPES_H
#define PET_BEHAVIOR_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include "animation/Animation.h"
#include "pet/PetBehaviorActionTypes.h"
#include "controller/StatusSetContract.h"
#include "controller/SystemCommandCatalog.h"
#include "pet/RuntimeValueResolver.h"
#include "common/AppProfile.h"

constexpr uint8_t kPetBehaviorSlotCount = APP_MAX_PET_STATS;
constexpr uint8_t kMaxPetBehaviorStats = kPetBehaviorSlotCount;
static_assert(kPetBehaviorSlotCount > 0, "Pet Stat capacity must be positive.");
static_assert(kPetBehaviorSlotCount <= 10, "Pet Stat Slot tokens require one decimal digit.");
constexpr uint8_t kMaxRuntimePetStates = 16;
#if ENABLE_GUESS_GAME
constexpr uint8_t kPetBehaviorGuessOutcomeCount = 4;
constexpr uint8_t kMaxPetBehaviorGuessEffects = kPetBehaviorGuessOutcomeCount * kMaxPetBehaviorStats;
#endif
constexpr uint8_t kPetBehaviorButtonCount = 8;

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
    PetBehaviorActionOutcomeConfig actionOutcomes[kMaxPetBehaviorActionOutcomes];
    PetBehaviorActionConditionConfig actionConditions[kMaxPetBehaviorActionConditions];
    PetBehaviorActionEffectConfig actionEffects[kMaxPetBehaviorActionEffects];
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
    uint8_t actionOutcomeCount;
#if ENABLE_GUESS_GAME
    uint8_t guessEffectCount;
#endif
    uint8_t buttonCount;
};

#endif // PET_BEHAVIOR_TYPES_H
