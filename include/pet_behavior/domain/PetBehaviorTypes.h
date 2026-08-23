#ifndef PET_BEHAVIOR_TYPES_H
#define PET_BEHAVIOR_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include "commands/domain/StatusSetContract.h"
#include "shared/config/AppProfile.h"

constexpr uint8_t kPetBehaviorSlotCount = 8;
constexpr uint8_t kMaxPetBehaviorStats = 6;
constexpr uint8_t kMaxPetBehaviorIdleTriggers = 16;
constexpr uint8_t kMaxPetBehaviorActions = 8;
constexpr uint8_t kMaxPetBehaviorActionEffects = kMaxPetBehaviorActions * kMaxPetBehaviorStats;
#if ENABLE_GUESS_GAME
constexpr uint8_t kPetBehaviorGuessOutcomeCount = 4;
constexpr uint8_t kMaxPetBehaviorGuessEffects = kPetBehaviorGuessOutcomeCount * kMaxPetBehaviorStats;
#endif
constexpr uint8_t kPetBehaviorButtonCount = 8;
constexpr size_t kPetBehaviorAnimationTokenSize = 8;
constexpr size_t kPetBehaviorSystemCommandTokenSize = 16;
constexpr size_t kPetBehaviorStatNameSize = 17;
constexpr size_t kMaxPetBehaviorContractBytes = 6144;

enum class PetBehaviorEffectOperation : uint8_t
{
    Change,
    Set,
};

struct PetBehaviorStatConfig
{
    bool active;
    char name[kPetBehaviorStatNameSize];
    int16_t initialValue;
    int16_t minValue;
    int16_t maxValue;
    int16_t dailyChange;
};

enum class PetBehaviorIdleTriggerOperator : uint8_t
{
    LessThan,
    GreaterThan,
};

struct PetBehaviorIdleTriggerConfig
{
    bool active;
    uint8_t statSlot;
    PetBehaviorIdleTriggerOperator comparison;
    int16_t threshold;
    char animation[kPetBehaviorAnimationTokenSize];
};

struct PetBehaviorActionConfig
{
    bool active;
    char animation[kPetBehaviorAnimationTokenSize];
    uint8_t playbackCount;
    uint8_t suspendDailyChangeDays;
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
    char systemCommand[kPetBehaviorSystemCommandTokenSize];
};

struct PetBehaviorConfig
{
    uint32_t schemaFingerprint;
    PetBehaviorStatConfig stats[kPetBehaviorSlotCount];
    PetBehaviorIdleTriggerConfig idleTriggers[kMaxPetBehaviorIdleTriggers];
    PetBehaviorActionConfig actions[kPetBehaviorSlotCount];
    PetBehaviorActionEffectConfig actionEffects[kMaxPetBehaviorActionEffects];
#if ENABLE_GUESS_GAME
    PetBehaviorGuessEffectConfig guessEffects[kMaxPetBehaviorGuessEffects];
#endif
    PetBehaviorButtonConfig buttons[kPetBehaviorButtonCount];
    StatusSetsConfig statusSets;
    char idleAnimation[kPetBehaviorAnimationTokenSize];
    uint8_t statCount;
    uint8_t idleTriggerCount;
    uint8_t actionCount;
    uint8_t actionEffectCount;
#if ENABLE_GUESS_GAME
    uint8_t guessEffectCount;
#endif
    uint8_t buttonCount;
};

#endif // PET_BEHAVIOR_TYPES_H
