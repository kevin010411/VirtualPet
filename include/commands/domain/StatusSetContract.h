#ifndef STATUS_SET_CONTRACT_H
#define STATUS_SET_CONTRACT_H

#include <stddef.h>
#include <stdint.h>
#include "shared/assets/AssetRuntimeContract.h"

constexpr uint8_t kMaxStatusSets = 5;
constexpr uint8_t kMaxStatusConditions = 3;

enum class StatusConditionSource : uint8_t
{
    PetStat,
    StageDays,
    PetStatus,
};

struct StatusSetCondition
{
    StatusConditionSource source;
    uint8_t statSlot;
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

bool resolveStatusSet(
    const StatusSetConfig &set,
    StatusValueSource valueSource,
    const void *valueContext,
    StatusSetResolution &resolution);

#endif // STATUS_SET_CONTRACT_H
