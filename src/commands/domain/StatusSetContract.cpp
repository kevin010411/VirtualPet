#include "commands/domain/StatusSetContract.h"

namespace
{
uint8_t levelForValue(int32_t value, int32_t minValue, int32_t maxValue, uint8_t levels)
{
    if (levels <= 1 || value <= minValue)
        return 0;
    if (value >= maxValue)
        return static_cast<uint8_t>(levels - 1);
    const int64_t valueCount = static_cast<int64_t>(maxValue) - minValue + 1;
    const int64_t offset = static_cast<int64_t>(value) - minValue;
    const int64_t level = offset * static_cast<int64_t>(levels) / valueCount;
    return static_cast<uint8_t>(
        level < static_cast<int64_t>(levels) ? level : levels - 1);
}
} // namespace

bool resolveStatusSet(
    const StatusSetConfig &set,
    StatusValueSource valueSource,
    const void *valueContext,
    StatusSetResolution &resolution)
{
    resolution = {};
    if (!set.animation.valid())
        return false;
    resolution.animation = set.animation;
    if (set.conditionCount == 0)
    {
        if (set.versionCount != 1)
            return false;
        resolution.playOnce = true;
        resolution.requiredVersions = 1;
        return true;
    }
    if (set.conditionCount > kMaxStatusConditions || valueSource == nullptr)
        return false;

    uint16_t version = 0;
    uint16_t requiredVersions = 1;
    for (uint8_t index = 0; index < set.conditionCount; ++index)
    {
        const StatusSetCondition &condition = set.conditions[index];
        int32_t value = 0;
        if (!valueSource(condition, valueContext, value))
            return false;
        if (condition.levels == 0)
            return false;
        version = static_cast<uint16_t>(
            version * condition.levels +
            levelForValue(value, condition.minValue, condition.maxValue, condition.levels));
        requiredVersions = static_cast<uint16_t>(requiredVersions * condition.levels);
        if (requiredVersions > AssetData::kMaxVersions)
            return false;
    }

    if (requiredVersions != set.versionCount || version >= requiredVersions)
        return false;
    resolution.versionIndex = static_cast<uint8_t>(version);
    resolution.requiredVersions = requiredVersions;
    resolution.playOnce = true;
    return true;
}
