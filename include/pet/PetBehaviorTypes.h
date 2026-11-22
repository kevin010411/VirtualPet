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
#include "audio/SequenceAudio.h"
#if ENABLE_INSTRUCTION_RESPONSE
#include "game/InstructionResponseSettings.h"
#include "game/TrainingCourse.h"
#endif

constexpr uint8_t kPetBehaviorSlotCount = APP_MAX_PET_STATS;
constexpr uint8_t kMaxPetBehaviorStats = kPetBehaviorSlotCount;
static_assert(kPetBehaviorSlotCount > 0, "Pet Stat capacity must be positive.");
static_assert(kPetBehaviorSlotCount <= 12, "Pet Stat capacity must fit the runtime contract.");
constexpr uint8_t kMaxRuntimePetStates = 16;
#if ENABLE_GUESS_GAME
constexpr uint8_t kPetBehaviorGuessOutcomeCount = 4;
constexpr uint8_t kMaxPetBehaviorGuessEffects = kPetBehaviorGuessOutcomeCount * kMaxPetBehaviorStats;
#endif
constexpr uint8_t kPetBehaviorButtonCount = 8;

struct PetBehaviorStatConfig
{
    bool active;
    uint32_t initialValue;
    uint32_t minValue;
    uint32_t maxValue;
    int64_t dailyChange;
};

#if ENABLE_INSTRUCTION_RESPONSE
struct PetBehaviorInstructionEffect
{
    uint8_t group;
    uint8_t statSlot;
    PetBehaviorEffectOperation operation;
    int64_t value;
};
#endif

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

enum class PetBehaviorButtonKind : uint8_t
{
    Empty,
    UserAction,
    SystemCommand,
    Menu,
};

struct PetBehaviorButtonConfig
{
    bool active;
    PetBehaviorButtonKind kind;
    uint8_t actionSlot;
    RuntimeSystemCommandId systemCommandId;
#if ENABLE_PROJECT_MENUS
    uint8_t menuSlot;
#endif
};

constexpr uint8_t kMaxScreenBlocks = 32;
constexpr uint16_t kMaxScreenRules = APP_MAX_SCREEN_RULES;
constexpr uint8_t kMaxScreenRulesPerBlock = 50;
static_assert(kMaxScreenRules > 0 && kMaxScreenRules <= 1600,
              "Screen Rule capacity must fit 32 blocks of 50 segments.");
constexpr uint8_t kUnboundScreenSource = 255;

enum class ScreenBlockKind : uint8_t
{
    Animation = 1,
    Button = 2,
    Stat = 3,
    StatSum = 4,
    StatDimensions = 5,
    StatSumSegments = 6,
};

struct ScreenBlockConfig
{
    ScreenBlockKind kind;
    uint8_t source;
    uint8_t x;
    uint8_t y;
    uint8_t width;
    uint8_t height;
    uint16_t firstRule;
    uint8_t ruleCount;
    uint16_t fallbackFrame;
    uint32_t numericSettings = 0;

    bool isNumeric() const
    {
        return kind == ScreenBlockKind::Stat || kind == ScreenBlockKind::StatSum ||
               kind == ScreenBlockKind::StatDimensions || kind == ScreenBlockKind::StatSumSegments;
    }
};

struct ScreenRuleConfig
{
    uint32_t minimum;
    uint32_t maximum;
    uint16_t frame;
};
static_assert(sizeof(ScreenRuleConfig) == 12, "Screen rules store exported unsigned Stat ranges.");

struct PetBehaviorConfig
{
    bool manualDays;
    uint8_t dayResetCount;
    PetBehaviorActionEffectConfig dayResets[kPetBehaviorSlotCount];
    uint32_t actionGatesOffset;
    uint16_t actionGateCount;
#if ENABLE_STAT_EVENTS
    bool persistentStatEvents;
    uint32_t statEventTriggersOffset;
    uint32_t statEventNodesOffset;
    uint32_t statEventEdgesOffset;
    uint32_t statEventConditionsOffset;
    uint32_t statEventEffectsOffset;
    uint32_t statEventPagesOffset;
    uint16_t statEventTriggerCount;
    uint16_t statEventNodeCount;
    uint16_t statEventPageCount;
#endif
#if ENABLE_SHOP
    bool shopEnabled;
#endif
#if ENABLE_TRAINING_COURSES
    TrainingCourseSettings trainingSettings;
    AssetData::AnimationRef trainingFrames;
#endif
#if ENABLE_LEGACY_INSTRUCTION_RESPONSE
    InstructionResponseSettings instructionSettings;
    AssetData::AnimationRef instructionOptions[3];
    PetBehaviorInstructionEffect instructionEffects[2 * kPetBehaviorSlotCount];
    uint8_t instructionEffectCount;
#endif
    bool deathEnabled;
    uint16_t deathStatMask;
    AssetData::AnimationRef deathAnimation;
    AssetData::RuntimeManifest assetManifest;
    AssetData::AnimationRef systemAnimations[kFirmwarePlaybackRoleCount];
    AssetData::AnimationRef layoutUnselected;
    AssetData::AnimationRef layoutSelected;
    uint32_t schemaFingerprint;
    uint32_t collectionFingerprint;
#if ENABLE_COLLECTION_GAME_EVENTS
    uint64_t captureOnlyMask;
    uint32_t collectionProgressFingerprint;
    bool retainCollectionOnRestart;
#endif
    uint32_t trackedActionFingerprint;
    uint8_t trackedActionCount;
    uint8_t trackedActions[8];
    PetBehaviorStatConfig stats[kPetBehaviorSlotCount];
    RuntimePetStateConfig petStates[kMaxRuntimePetStates];
    PetBehaviorActionConfig actions[kMaxPetBehaviorActions];
#if ENABLE_SEQUENCE_AUDIO
    // Runtime animation IDs are stable across appearances and Versions.
    SequenceAudioRef animationAudio[AssetData::kMaxRuntimeAnimationId];
#endif
    PetBehaviorAnimationPlaybackConfig followingAnimations[kMaxPetBehaviorFollowingAnimations];
    PetBehaviorActionOutcomeConfig actionOutcomes[kMaxPetBehaviorActionOutcomes];
    PetBehaviorActionConditionConfig actionConditions[kMaxPetBehaviorActionConditions];
    PetBehaviorActionEffectConfig actionEffects[kStoredPetBehaviorActionEffects];
    uint32_t actionEffectsOffset;
    bool actionEffectsStreamed;
#if ENABLE_GUESS_GAME
    PetBehaviorGuessEffectConfig guessEffects[kMaxPetBehaviorGuessEffects];
#endif
    PetBehaviorButtonConfig buttons[kPetBehaviorButtonCount];
    ScreenBlockConfig screenBlocks[kMaxScreenBlocks];
    uint8_t screenBlockCount;
    ScreenRuleConfig screenRules[kMaxScreenRules];
    uint16_t screenRuleCount;
    StatusSetsConfig statusSets;
    AssetData::AnimationRef idleAnimation;
    uint8_t activeSpeciesSlot;
    uint8_t activeOutfitSlot;
    uint8_t statCount;
    uint8_t petStateCount;
    uint8_t actionCount;
    uint8_t actionConditionCount;
    uint16_t actionEffectCount;
    uint8_t actionOutcomeCount;
#if ENABLE_GUESS_GAME
    uint8_t guessEffectCount;
#endif
    uint8_t buttonCount;
};

#endif // PET_BEHAVIOR_TYPES_H
