#ifndef PET_BEHAVIOR_TYPES_H
#define PET_BEHAVIOR_TYPES_H

#include <stddef.h>
#include <stdint.h>

constexpr uint8_t kPetBehaviorSlotCount = 8;
constexpr uint8_t kMaxPetBehaviorStats = 6;
constexpr uint8_t kMaxPetBehaviorIdleTriggers = 16;
constexpr uint8_t kMaxPetBehaviorActions = 8;
constexpr uint8_t kMaxPetBehaviorActionEffects = kMaxPetBehaviorActions * kMaxPetBehaviorStats;
constexpr uint8_t kPetBehaviorButtonCount = 8;
constexpr size_t kPetBehaviorAnimationTokenSize = 8;
constexpr size_t kPetBehaviorSystemCommandTokenSize = 16;
constexpr size_t kPetBehaviorStatNameSize = 17;
constexpr size_t kMaxPetBehaviorContractBytes = 4096;

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
};

struct PetBehaviorActionEffectConfig
{
    bool active;
    uint8_t actionSlot;
    uint8_t statSlot;
    enum class Operation : uint8_t
    {
        Change,
        Set,
    } operation;
    int16_t value;
};

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
    uint32_t schemaRevision;
    PetBehaviorStatConfig stats[kPetBehaviorSlotCount];
    PetBehaviorIdleTriggerConfig idleTriggers[kMaxPetBehaviorIdleTriggers];
    PetBehaviorActionConfig actions[kPetBehaviorSlotCount];
    PetBehaviorActionEffectConfig actionEffects[kMaxPetBehaviorActionEffects];
    PetBehaviorButtonConfig buttons[kPetBehaviorButtonCount];
    char idleAnimation[kPetBehaviorAnimationTokenSize];
    uint8_t statCount;
    uint8_t idleTriggerCount;
    uint8_t actionCount;
    uint8_t actionEffectCount;
    uint8_t buttonCount;
};

#endif // PET_BEHAVIOR_TYPES_H
