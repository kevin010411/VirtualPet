#include "commands/domain/StatusSetContract.h"

#include <string.h>

namespace
{
bool copyText(char *destination, size_t destinationSize, const char *source)
{
    if (destination == nullptr || destinationSize == 0 || source == nullptr || source[0] == '\0')
        return false;
    const size_t length = strlen(source);
    if (length >= destinationSize)
        return false;
    memcpy(destination, source, length + 1);
    return true;
}

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
    if (!copyText(resolution.animation, sizeof(resolution.animation), set.animation))
        return false;
    if (set.conditionCount == 0)
    {
        if (strcmp(set.animation, "Status") != 0)
            return false;
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
        if (!valueSource(condition.source, valueContext, value))
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
