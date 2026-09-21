#ifndef STATUS_SET_CONTRACT_H
#define STATUS_SET_CONTRACT_H

#include <stddef.h>
#include <stdint.h>
#include "shared/assets/AssetRuntimeContract.h"
#include "pet_behavior/domain/RuntimeValueResolver.h"

struct ActivePetState;

constexpr uint8_t kMaxStatusSets = 5;
constexpr uint8_t kMaxStatusConditions = 3;

enum class StatusConditionKind : uint8_t
{
    RuntimeValue,
    PetStatusAxis,
};

struct StatusSetCondition
{
    StatusConditionKind kind;
    RuntimeValueId valueId;
    uint16_t petStateMask;
    uint8_t levels;
    int32_t minValue;
    int32_t maxValue;
};

struct StatusSetConfig
{
    AssetData::AnimationRef animation;
    StatusSetCondition conditions[kMaxStatusConditions];
    uint8_t conditionCount;
    uint16_t versionCount;
};

struct StatusSetsConfig
{
    StatusSetConfig sets[kMaxStatusSets];
    uint8_t count;
};

using StatusValueSource = bool (*)(const StatusSetCondition &condition,
                                  const void *context,
                                  int32_t &value);

struct StatusSetResolution
{
    AssetData::AnimationRef animation;
    uint8_t versionIndex;
    uint16_t requiredVersions;
    bool playOnce;
};

bool resolvePetStateStatusLevel(
    const StatusSetCondition &condition,
    const ActivePetState &activePetState,
    int32_t &level);

bool resolveStatusSet(
    const StatusSetConfig &set,
    StatusValueSource valueSource,
    const void *valueContext,
    StatusSetResolution &resolution);

#endif // STATUS_SET_CONTRACT_H
