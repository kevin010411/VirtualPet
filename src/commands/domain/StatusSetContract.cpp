#include "commands/domain/StatusSetContract.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

namespace
{
struct SourceDefinition
{
    const char *source;
    const char *token;
};

constexpr SourceDefinition kBuiltInSources[] = {
    {"age", "Age"},
    {"clean", "Clean"},
    {"env", "Env"},
    {"health", "Health"},
    {"hunger", "Hungry"},
    {"mood", "Mood"},
    {"sick", "Sick"},
    {"stage_days", "StageDays"},
};

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

bool appendText(char *destination, size_t destinationSize, const char *suffix)
{
    const size_t currentLength = strlen(destination);
    const size_t suffixLength = strlen(suffix);
    if (currentLength + suffixLength >= destinationSize)
        return false;
    memcpy(destination + currentLength, suffix, suffixLength + 1);
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

bool customSourceIndex(const char *source, uint8_t &index)
{
    constexpr const char *prefix = "custom";
    if (source == nullptr || strncmp(source, prefix, strlen(prefix)) != 0)
        return false;
    const char *digit = source + strlen(prefix);
    if (digit[0] < '0' || digit[0] > '7' || digit[1] != '\0')
        return false;
    index = static_cast<uint8_t>(digit[0] - '0');
    return true;
}

bool sourceDefinition(const char *source, uint8_t &rank, char *token, size_t tokenSize)
{
    for (uint8_t index = 0; index < sizeof(kBuiltInSources) / sizeof(kBuiltInSources[0]); ++index)
    {
        if (strcmp(source, kBuiltInSources[index].source) != 0)
            continue;
        rank = index;
        return copyText(token, tokenSize, kBuiltInSources[index].token);
    }

    uint8_t customIndex = 0;
    if (!customSourceIndex(source, customIndex))
        return false;
    rank = static_cast<uint8_t>(sizeof(kBuiltInSources) / sizeof(kBuiltInSources[0]) + customIndex);
    char customToken[] = "Custom0";
    customToken[6] = static_cast<char>('0' + customIndex);
    return copyText(token, tokenSize, customToken);
}

bool parseCondition(char *descriptor, StatusSetCondition &condition, uint8_t &rank, char *token, size_t tokenSize)
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
    if (!sourceDefinition(fields[0], rank, token, tokenSize) ||
        !copyText(condition.source, sizeof(condition.source), fields[0]) ||
        !parseInt32(fields[1], levels) ||
        !parseInt32(fields[2], condition.minValue) ||
        !parseInt32(fields[3], condition.maxValue) ||
        levels < 1 || levels > 32 ||
        condition.minValue >= condition.maxValue)
        return false;

    condition.levels = static_cast<uint8_t>(levels);
    return strcmp(condition.source, "sick") != 0 || condition.levels == 2;
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

    char expectedAnimation[kStatusAnimationNameSize] = "Status";
    uint8_t previousRank = 0;
    bool hasPreviousRank = false;
    uint16_t frameProduct = 1;
    char *cursor = descriptors;
    while (cursor != nullptr)
    {
        if (set.conditionCount >= kMaxStatusConditions)
            return false;
        char *next = strchr(cursor, ',');
        if (next != nullptr)
            *next = '\0';

        uint8_t rank = 0;
        char token[12] = {};
        StatusSetCondition &condition = set.conditions[set.conditionCount];
        if (!parseCondition(trim(cursor), condition, rank, token, sizeof(token)) ||
            (hasPreviousRank && rank <= previousRank) ||
            !appendText(expectedAnimation, sizeof(expectedAnimation), token))
            return false;

        frameProduct = static_cast<uint16_t>(frameProduct * condition.levels);
        if (frameProduct > 256)
            return false;
        previousRank = rank;
        hasPreviousRank = true;
        ++set.conditionCount;
        cursor = next == nullptr ? nullptr : next + 1;
        if (cursor != nullptr && trim(cursor)[0] == '\0')
            return false;
    }

    return set.conditionCount > 0 && strcmp(set.animation, expectedAnimation) == 0;
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
