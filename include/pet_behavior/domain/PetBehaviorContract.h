#ifndef PET_BEHAVIOR_CONTRACT_H
#define PET_BEHAVIOR_CONTRACT_H

#include <stddef.h>
#include <stdint.h>
#include <SdFat.h>

constexpr uint8_t kPetBehaviorSlotCount = 8;
constexpr uint8_t kMaxPetBehaviorStats = 6;
constexpr uint8_t kMaxPetBehaviorHealthStatuses = 6;
constexpr uint8_t kMaxPetBehaviorActions = 8;
constexpr uint8_t kMaxPetBehaviorActionEffects = kMaxPetBehaviorActions * kMaxPetBehaviorStats;
constexpr uint8_t kPetBehaviorButtonCount = 8;
constexpr size_t kPetBehaviorAnimationTokenSize = 8;
constexpr size_t kPetBehaviorSystemCommandTokenSize = 16;
constexpr size_t kMaxPetBehaviorContractBytes = 4096;

struct PetBehaviorStatConfig
{
    bool active;
    int16_t initialValue;
    int16_t minValue;
    int16_t maxValue;
    int16_t dailyChange;
};

struct PetBehaviorHealthStatusConfig
{
    bool active;
    uint8_t statSlot;
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
    int16_t delta;
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
    PetBehaviorHealthStatusConfig healthStatuses[kMaxPetBehaviorHealthStatuses];
    PetBehaviorActionConfig actions[kPetBehaviorSlotCount];
    PetBehaviorActionEffectConfig actionEffects[kMaxPetBehaviorActionEffects];
    PetBehaviorButtonConfig buttons[kPetBehaviorButtonCount];
    char idleAnimation[kPetBehaviorAnimationTokenSize];
    uint8_t statCount;
    uint8_t healthStatusCount;
    uint8_t actionCount;
    uint8_t actionEffectCount;
    uint8_t buttonCount;
};

// The parser validates the complete v1 contract before assigning config.
bool parsePetBehaviorContract(const char *contractText, PetBehaviorConfig &config);

// The SD loader streams the contract through the same parser and retains no source text.
bool loadPetBehaviorContract(SdFat *sd, PetBehaviorConfig &config);

#endif // PET_BEHAVIOR_CONTRACT_H
