#include "commands/domain/StatusSetContract.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

namespace
{
bool isSpace(char value)
{
    return value == ' ' || value == '\t';
}

char *trim(char *text)
{
    if (text == nullptr)
        return nullptr;
    while (isSpace(*text))
        ++text;
    char *end = text + strlen(text);
    while (end > text && isSpace(end[-1]))
        --end;
    *end = '\0';
    return text;
}

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

bool parseInt32(const char *text, int32_t &value)
{
    if (text == nullptr || text[0] == '\0')
        return false;
    errno = 0;
    char *end = nullptr;
    const long parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE ||
        parsed < INT32_MIN || parsed > INT32_MAX)
        return false;
    value = static_cast<int32_t>(parsed);
    return true;
}

bool validSourceName(const char *source)
{
    if (source == nullptr || source[0] == '\0')
        return false;
    if (strcmp(source, "stage_days") == 0)
        return true;
    if (source[0] < 'a' || source[0] > 'z')
        return false;
    for (const char *cursor = source + 1; *cursor != '\0'; ++cursor)
    {
        if ((*cursor < 'a' || *cursor > 'z') &&
            (*cursor < '0' || *cursor > '9') && *cursor != '_')
            return false;
    }
    return true;
}

bool parseCondition(char *descriptor, StatusSetCondition &condition)
{
    char *fields[4] = {};
    char *cursor = descriptor;
    for (uint8_t index = 0; index < 4; ++index)
    {
        fields[index] = cursor;
        char *separator = strchr(cursor, ':');
        if (index < 3)
        {
            if (separator == nullptr)
                return false;
            *separator = '\0';
            cursor = separator + 1;
        }
        else if (separator != nullptr)
        {
            return false;
        }
        fields[index] = trim(fields[index]);
        if (fields[index][0] == '\0')
            return false;
    }

    int32_t levels = 0;
    if (!validSourceName(fields[0]) ||
        !copyText(condition.source, sizeof(condition.source), fields[0]) ||
        !parseInt32(fields[1], levels) ||
        !parseInt32(fields[2], condition.minValue) ||
        !parseInt32(fields[3], condition.maxValue) ||
        levels < 1 || levels > 32 ||
        condition.minValue >= condition.maxValue)
        return false;

    condition.levels = static_cast<uint8_t>(levels);
    return true;
}

bool sameConditionSet(const StatusSetConfig &left, const StatusSetConfig &right)
{
    if (left.conditionCount != right.conditionCount)
        return false;
    for (uint8_t index = 0; index < left.conditionCount; ++index)
    {
        if (strcmp(left.conditions[index].source, right.conditions[index].source) != 0)
            return false;
    }
    return true;
}

bool parseSetRow(char *row, StatusSetConfig &set)
{
    char *separator = strchr(row, '|');
    if (separator == nullptr || strchr(separator + 1, '|') != nullptr)
        return false;
    *separator = '\0';

    char *animation = trim(row);
    char *descriptors = trim(separator + 1);
    if (!copyText(set.animation, sizeof(set.animation), animation))
        return false;

    if (strcmp(set.animation, "Status") == 0)
        return descriptors[0] == '\0';
    if (descriptors[0] == '\0')
        return false;

    uint16_t frameProduct = 1;
    char *cursor = descriptors;
    while (cursor != nullptr)
    {
        if (set.conditionCount >= kMaxStatusConditions)
            return false;
        char *next = strchr(cursor, ',');
        if (next != nullptr)
            *next = '\0';

        StatusSetCondition &condition = set.conditions[set.conditionCount];
        if (!parseCondition(trim(cursor), condition))
            return false;
        for (uint8_t index = 0; index < set.conditionCount; ++index)
        {
            if (strcmp(set.conditions[index].source, condition.source) == 0)
                return false;
        }

        frameProduct = static_cast<uint16_t>(frameProduct * condition.levels);
        if (frameProduct > 256)
            return false;
        ++set.conditionCount;
        cursor = next == nullptr ? nullptr : next + 1;
        if (cursor != nullptr && trim(cursor)[0] == '\0')
            return false;
    }

    return set.conditionCount > 0 && strncmp(set.animation, "Status", 6) == 0;
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

bool parseStatusSetsContract(const char *contractText, StatusSetsConfig &config)
{
    config = {};
    if (contractText == nullptr)
        return false;
    const size_t length = strlen(contractText);
    if (length == 0 || length >= kMaxStatusContractBytes)
        return false;

    char working[kMaxStatusContractBytes] = {};
    memcpy(working, contractText, length + 1);

    bool versionSeen = false;
    char *cursor = working;
    while (cursor != nullptr)
    {
        char *next = strchr(cursor, '\n');
        if (next != nullptr)
        {
            *next = '\0';
            ++next;
        }
        const size_t rowLength = strlen(cursor);
        if (rowLength > 0 && cursor[rowLength - 1] == '\r')
            cursor[rowLength - 1] = '\0';
        char *row = trim(cursor);
        if (row[0] != '\0' && row[0] != '#')
        {
            if (!versionSeen)
            {
                if (strcmp(row, "version=1") != 0)
                    return false;
                versionSeen = true;
            }
            else
            {
                if (config.count >= kMaxStatusSets ||
                    !parseSetRow(row, config.sets[config.count]))
                    return false;
                for (uint8_t index = 0; index < config.count; ++index)
                {
                    if (sameConditionSet(config.sets[index], config.sets[config.count]))
                        return false;
                }
                ++config.count;
            }
        }
        cursor = next;
    }

    return versionSeen && config.count >= 1 && config.count <= kMaxStatusSets;
}

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
