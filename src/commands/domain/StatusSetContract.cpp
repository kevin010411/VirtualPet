#include "commands/domain/StatusSetContract.h"

namespace
{
uint8_t levelForValue(int32_t value, int32_t minValue, int32_t maxValue, uint8_t levels)
{
    if (levels <= 1 || value <= minValue)
        return 0;
    if (value >= maxValue)
        return static_cast<uint8_t>(levels - 1);
    const int64_t range = static_cast<int64_t>(maxValue) - minValue;
    const int64_t offset = static_cast<int64_t>(value) - minValue;
    return static_cast<uint8_t>(
        (offset * static_cast<int64_t>(levels - 1) + (range - 1)) / range);
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
        resolution.playOnce = true;
        return true;
    }
    if (set.conditionCount > kMaxStatusConditions || valueSource == nullptr)
        return false;

    uint16_t frame = 0;
    uint16_t requiredFrames = 1;
    for (uint8_t index = 0; index < set.conditionCount; ++index)
    {
        const StatusSetCondition &condition = set.conditions[index];
        int32_t value = 0;
        if (!valueSource(condition, valueContext, value))
            return false;
        frame = static_cast<uint16_t>(
            frame * condition.levels +
            levelForValue(value, condition.minValue, condition.maxValue, condition.levels));
        requiredFrames = static_cast<uint16_t>(requiredFrames * condition.levels);
        if (requiredFrames > 256)
            return false;
    }

    resolution.frame = static_cast<uint16_t>(frame + 1);
    resolution.requiredFrames = requiredFrames;
    return true;
}
