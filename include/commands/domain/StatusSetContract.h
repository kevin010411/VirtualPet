#ifndef STATUS_SET_CONTRACT_H
#define STATUS_SET_CONTRACT_H

#include <stddef.h>
#include <stdint.h>
#include "shared/assets/AssetRuntimeContract.h"

constexpr size_t kStatusSourceNameSize = 17;
constexpr uint8_t kMaxStatusSets = 5;
constexpr uint8_t kMaxStatusConditions = 3;

struct StatusSetCondition
{
    char source[kStatusSourceNameSize];
    uint8_t levels;
    int32_t minValue;
    int32_t maxValue;
};

struct StatusSetConfig
{
    AssetData::AnimationRef animation;
    StatusSetCondition conditions[kMaxStatusConditions];
    uint8_t conditionCount;
};

struct StatusSetsConfig
{
    StatusSetConfig sets[kMaxStatusSets];
    uint8_t count;
};

using StatusValueSource = bool (*)(const char *source, const void *context, int32_t &value);

struct StatusSetResolution
{
    AssetData::AnimationRef animation;
    uint16_t frame;
    uint16_t requiredFrames;
    bool playOnce;
};

bool resolveStatusSet(
    const StatusSetConfig &set,
    StatusValueSource valueSource,
    const void *valueContext,
    StatusSetResolution &resolution);

#endif // STATUS_SET_CONTRACT_H
