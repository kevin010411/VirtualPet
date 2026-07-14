#include "appearance/adapters/SdAppearanceLoader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared/utils/TextBuffer.h"

namespace
{
constexpr const char *kEvolutionRulesPath = "/evolution_rules.txt";
constexpr const char *kStateSchemaPath = "/state_schema.txt";
constexpr int32_t kMinConditionValue = -2147483647L - 1L;
constexpr int32_t kMaxConditionValue = 2147483647L;
constexpr size_t kMaxStateAliases = 8;
constexpr size_t kMaxStatNameLength = 23;

struct StateAlias
{
    char name[kMaxStatNameLength + 1];
    uint8_t customIndex;
    int32_t minValue;
    int32_t maxValue;
    int32_t defaultValue;
};

bool isSpaceChar(char c)
{
    return c == ' ' || c == '\t';
}

char *trimField(char *text)
{
    while (text != nullptr && isSpaceChar(*text))
        ++text;

    if (text == nullptr || *text == '\0')
        return text;

    char *end = text + strlen(text) - 1;
    while (end >= text && isSpaceChar(*end))
    {
        *end = '\0';
        --end;
    }
    return text;
}

bool readConfigLine(File &file, char *line, size_t lineSize)
{
    if (line == nullptr || lineSize == 0)
        return false;

    size_t index = 0;
    bool sawAny = false;
    while (file.available())
    {
        const char c = static_cast<char>(file.read());
        sawAny = true;
        if (c == '\r')
            continue;
        if (c == '\n')
            break;
        if (index + 1 < lineSize)
            line[index++] = c;
    }

    line[index] = '\0';
    return sawAny || index > 0;
}

bool parseUnsignedField(const char *text, uint32_t &value)
{
    if (text == nullptr || text[0] == '\0')
        return false;

    uint32_t parsed = 0;
    for (const char *cursor = text; *cursor != '\0'; ++cursor)
    {
        if (*cursor < '0' || *cursor > '9')
            return false;
        parsed = parsed * 10UL + static_cast<uint32_t>(*cursor - '0');
    }

    value = parsed;
    return true;
}

bool parseSignedField(const char *text, int32_t &value)
{
    if (text == nullptr || text[0] == '\0')
        return false;

    bool negative = false;
    const char *cursor = text;
    if (*cursor == '-')
    {
        negative = true;
        ++cursor;
    }
    if (*cursor == '\0')
        return false;

    int32_t parsed = 0;
    for (; *cursor != '\0'; ++cursor)
    {
        if (*cursor < '0' || *cursor > '9')
            return false;
        parsed = parsed * 10L + static_cast<int32_t>(*cursor - '0');
    }

    value = negative ? -parsed : parsed;
    return true;
}

bool isValidAppearanceCode(const char *code)
{
    if (code == nullptr || code[0] == '\0')
        return false;

    const size_t len = strlen(code);
    if (len > 8)
        return false;

    for (size_t i = 0; i < len; ++i)
    {
        const char c = code[i];
        const bool valid = (c >= 'a' && c <= 'z') ||
                           (c >= 'A' && c <= 'Z') ||
                           (c >= '0' && c <= '9') ||
                           c == '_' || c == '-';
        if (!valid)
            return false;
    }

    return true;
}

bool isValidStatName(const char *name)
{
    if (name == nullptr || name[0] == '\0')
        return false;

    const size_t len = strlen(name);
    if (len > kMaxStatNameLength)
        return false;

    for (size_t i = 0; i < len; ++i)
    {
        const char c = name[i];
        const bool valid = (c >= 'a' && c <= 'z') ||
                           (c >= 'A' && c <= 'Z') ||
                           (c >= '0' && c <= '9') ||
                           c == '_' || c == '-';
        if (!valid)
            return false;
    }

    return true;
}

bool splitFields(char *line, char *fields[], size_t fieldCount)
{
    size_t count = 0;
    char *cursor = line;

    while (count < fieldCount)
    {
        fields[count++] = cursor;
        char *sep = strchr(cursor, '|');
        if (sep == nullptr)
            break;

        *sep = '\0';
        cursor = sep + 1;
    }

    if (count != fieldCount)
        return false;

    if (strchr(fields[fieldCount - 1], '|') != nullptr)
        return false;

    for (size_t i = 0; i < fieldCount; ++i)
    {
        fields[i] = trimField(fields[i]);
        if (fields[i] == nullptr)
            return false;
    }

    return true;
}

bool parseCustomSlot(const char *text, uint8_t &index)
{
    if (text == nullptr || strncmp(text, "custom", 6) != 0 || text[6] == '\0' || text[7] != '\0')
        return false;

    const char digit = text[6];
    if (digit < '0' || digit > '7')
        return false;

    index = static_cast<uint8_t>(digit - '0');
    return true;
}

bool parseStateSchemaRow(char *line, StateAlias &alias)
{
    char *fields[5] = {};
    if (!splitFields(line, fields, 5))
        return false;

    uint8_t customIndex = 0;
    int32_t minValue = 0;
    int32_t maxValue = 0;
    int32_t defaultValue = 0;
    if (!isValidStatName(fields[0]) ||
        !parseCustomSlot(fields[1], customIndex) ||
        !parseSignedField(fields[2], minValue) ||
        !parseSignedField(fields[3], maxValue) ||
        !parseSignedField(fields[4], defaultValue) ||
        minValue > maxValue ||
        defaultValue < minValue ||
        defaultValue > maxValue)
    {
        return false;
    }

    strncpy(alias.name, fields[0], sizeof(alias.name) - 1);
    alias.name[sizeof(alias.name) - 1] = '\0';
    alias.customIndex = customIndex;
    alias.minValue = minValue;
    alias.maxValue = maxValue;
    alias.defaultValue = defaultValue;
    return true;
}

void loadStateAliases(SdFat *sd, StateAlias aliases[], size_t maxAliases, size_t &aliasCount)
{
    aliasCount = 0;
    if (sd == nullptr || aliases == nullptr || maxAliases == 0 || !sd->exists(kStateSchemaPath))
        return;

    File file = sd->open(kStateSchemaPath, FILE_READ);
    if (!file)
        return;

    char line[96] = {};
    while (readConfigLine(file, line, sizeof(line)) && aliasCount < maxAliases)
    {
        char *content = trimField(line);
        if (content == nullptr || content[0] == '\0' || content[0] == '#')
            continue;

        StateAlias alias = {};
        if (!parseStateSchemaRow(content, alias))
            continue;

        bool duplicate = false;
        for (size_t i = 0; i < aliasCount; ++i)
        {
            if (strcmp(aliases[i].name, alias.name) == 0)
            {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
            continue;

        aliases[aliasCount++] = alias;
    }

    file.close();
}

bool statValueByName(const char *name, const PetStatSnapshot &stats, const StateAlias aliases[], size_t aliasCount, int32_t &value)
{
    if (name == nullptr)
        return false;

    if (strcmp(name, "stage_days") == 0)
        value = static_cast<int32_t>(stats.stage_days);
    else if (strcmp(name, "health") == 0)
        value = stats.health;
    else if (strcmp(name, "age") == 0)
        value = stats.age;
    else if (strcmp(name, "hunger") == 0)
        value = stats.hunger;
    else if (strcmp(name, "mood") == 0)
        value = stats.mood;
    else if (strcmp(name, "clean") == 0)
        value = stats.clean;
    else if (strcmp(name, "env") == 0)
        value = stats.env;
    else if (strcmp(name, "sick") == 0)
        value = stats.sick;
    else if (strcmp(name, "status") == 0)
        value = stats.status;
    else
    {
        uint8_t customIndex = 0;
        if (parseCustomSlot(name, customIndex))
            value = stats.customStats[customIndex];
        else
        {
            for (size_t i = 0; i < aliasCount; ++i)
            {
                if (strcmp(name, aliases[i].name) != 0)
                    continue;

                value = stats.customStats[aliases[i].customIndex];
                return true;
            }
            return false;
        }
    }

    return true;
}

bool sourceMatches(const char *sourceSpecies, const PetStatSnapshot &stats)
{
    if (sourceSpecies == nullptr)
        return false;
    if (strcmp(sourceSpecies, "init") == 0)
        return false;
    if (strcmp(sourceSpecies, "*") == 0)
        return true;
    return strcmp(sourceSpecies, stats.species) == 0;
}

bool parseConditionRange(char *text, int32_t &minValue, int32_t &maxValue)
{
    text = trimField(text);
    if (text == nullptr || text[0] == '\0')
        return false;

    if (strcmp(text, "*") == 0)
    {
        minValue = kMinConditionValue;
        maxValue = kMaxConditionValue;
        return true;
    }

    char *rangeSep = strstr(text, "..");
    if (rangeSep == nullptr)
    {
        if (!parseSignedField(text, minValue))
            return false;
        maxValue = minValue;
        return true;
    }

    *rangeSep = '\0';
    char *minText = trimField(text);
    char *maxText = trimField(rangeSep + 2);
    if (strcmp(minText, "*") == 0)
        minValue = kMinConditionValue;
    else if (!parseSignedField(minText, minValue))
        return false;

    if (strcmp(maxText, "*") == 0)
        maxValue = kMaxConditionValue;
    else if (!parseSignedField(maxText, maxValue))
        return false;

    return minValue <= maxValue;
}

bool conditionMatches(char *condition,
                      const PetStatSnapshot &stats,
                      const StateAlias aliases[],
                      size_t aliasCount)
{
    char *equals = strchr(condition, '=');
    if (equals == nullptr)
        return false;

    *equals = '\0';
    char *name = trimField(condition);
    char *rangeText = trimField(equals + 1);
    if (!isValidStatName(name))
        return false;

    int32_t statValue = 0;
    int32_t minValue = 0;
    int32_t maxValue = 0;
    if (!statValueByName(name, stats, aliases, aliasCount, statValue) ||
        !parseConditionRange(rangeText, minValue, maxValue))
    {
        return false;
    }

    return statValue >= minValue && statValue <= maxValue;
}

bool conditionsMatch(char *conditions,
                     const PetStatSnapshot &stats,
                     const StateAlias aliases[],
                     size_t aliasCount)
{
    conditions = trimField(conditions);
    if (conditions == nullptr)
        return false;
    if (conditions[0] == '\0')
        return true;

    char *cursor = conditions;
    while (cursor != nullptr && *cursor != '\0')
    {
        char *sep = strchr(cursor, ',');
        if (sep != nullptr)
            *sep = '\0';

        char *condition = trimField(cursor);
        if (condition == nullptr || condition[0] == '\0' ||
            !conditionMatches(condition, stats, aliases, aliasCount))
        {
            return false;
        }

        cursor = (sep == nullptr) ? nullptr : sep + 1;
    }

    return true;
}

bool splitEvolutionRule(char *line, char *&sourceSpecies, char *&speciesCode, char *&outfitCode, char *&conditions)
{
    char *fields[4] = {};
    if (!splitFields(line, fields, 4))
        return false;

    sourceSpecies = fields[0];
    speciesCode = fields[1];
    outfitCode = fields[2];
    conditions = fields[3];
    return (strcmp(sourceSpecies, "*") == 0 || isValidAppearanceCode(sourceSpecies)) &&
           isValidAppearanceCode(speciesCode) &&
           isValidAppearanceCode(outfitCode);
}

bool buildSpeciesOutfitListPath(char *dest, size_t destSize, const char *speciesCode)
{
    if (dest == nullptr || destSize == 0 || !isValidAppearanceCode(speciesCode))
        return false;

    TextBuffer path(dest, destSize);
    return path.append("/index/") && path.append(speciesCode) && path.append(".txt") && path.ok();
}

bool buildOutfitPreviewPath(char *dest, size_t destSize, const char *speciesCode)
{
    if (dest == nullptr || destSize == 0 || !isValidAppearanceCode(speciesCode))
        return false;

    TextBuffer path(dest, destSize);
    return path.append("/index/") && path.append(speciesCode) && path.append("_outfit.txt") && path.ok();
}

bool copyText(char *dest, size_t destSize, const char *source)
{
    if (dest == nullptr || destSize == 0 || source == nullptr)
        return false;

    const size_t len = strlen(source);
    if (len >= destSize)
        return false;

    memcpy(dest, source, len + 1);
    return true;
}

bool splitOutfitPreviewRow(char *line, char *fields[], size_t fieldCount)
{
    size_t count = 0;
    char *cursor = line;

    while (count < fieldCount)
    {
        fields[count++] = cursor;
        char *sep = strchr(cursor, '|');
        if (sep == nullptr)
            break;

        *sep = '\0';
        cursor = sep + 1;
    }

    if (count != fieldCount)
        return false;

    if (strchr(fields[fieldCount - 1], '|') != nullptr)
        return false;

    for (size_t i = 0; i < fieldCount; ++i)
    {
        fields[i] = trimField(fields[i]);
        if (fields[i] == nullptr || fields[i][0] == '\0')
            return false;
    }

    return true;
}

} // namespace

SdAppearanceLoader::SdAppearanceLoader(SdFat *refSd) : sd(refSd)
{
}

bool SdAppearanceLoader::findInitialAppearance(AppearanceSelection &selection)
{
    selection = {};
    if (sd == nullptr || !sd->exists(kEvolutionRulesPath))
        return false;

    File file = sd->open(kEvolutionRulesPath, FILE_READ);
    if (!file)
        return false;

    char line[192] = {};
    while (readConfigLine(file, line, sizeof(line)))
    {
        char *content = trimField(line);
        if (content == nullptr || content[0] == '\0' || content[0] == '#')
            continue;

        char *speciesCode = nullptr;
        char *outfitCode = nullptr;
        char *sourceSpecies = nullptr;
        char *conditions = nullptr;
        if (!splitEvolutionRule(content, sourceSpecies, speciesCode, outfitCode, conditions))
            continue;

        if (strcmp(sourceSpecies, "init") != 0)
            continue;

        strncpy(selection.speciesCode, speciesCode, sizeof(selection.speciesCode) - 1);
        selection.speciesCode[sizeof(selection.speciesCode) - 1] = '\0';
        strncpy(selection.outfitCode, outfitCode, sizeof(selection.outfitCode) - 1);
        selection.outfitCode[sizeof(selection.outfitCode) - 1] = '\0';
        file.close();
        return true;
    }

    file.close();
    return false;
}

bool SdAppearanceLoader::findEvolutionTarget(const PetStatSnapshot &stats, AppearanceSelection &selection)
{
    selection = {};
    if (sd == nullptr || !sd->exists(kEvolutionRulesPath))
        return false;

    StateAlias aliases[kMaxStateAliases] = {};
    size_t aliasCount = 0;
    loadStateAliases(sd, aliases, kMaxStateAliases, aliasCount);

    File file = sd->open(kEvolutionRulesPath, FILE_READ);
    if (!file)
        return false;

    char line[192] = {};
    while (readConfigLine(file, line, sizeof(line)))
    {
        char *content = trimField(line);
        if (content == nullptr || content[0] == '\0' || content[0] == '#')
            continue;

        char *speciesCode = nullptr;
        char *outfitCode = nullptr;
        char *sourceSpecies = nullptr;
        char *conditions = nullptr;
        if (!splitEvolutionRule(content, sourceSpecies, speciesCode, outfitCode, conditions))
            continue;

        if (!sourceMatches(sourceSpecies, stats))
            continue;

        if (!conditionsMatch(conditions, stats, aliases, aliasCount))
            continue;

        strncpy(selection.speciesCode, speciesCode, sizeof(selection.speciesCode) - 1);
        selection.speciesCode[sizeof(selection.speciesCode) - 1] = '\0';
        strncpy(selection.outfitCode, outfitCode, sizeof(selection.outfitCode) - 1);
        selection.outfitCode[sizeof(selection.outfitCode) - 1] = '\0';
        file.close();
        return true;
    }

    file.close();
    return false;
}

bool SdAppearanceLoader::loadSpecies(char species[][9], size_t maxSpecies, size_t &speciesCount)
{
    speciesCount = 0;
    if (sd == nullptr || species == nullptr || maxSpecies == 0 || !sd->exists(kEvolutionRulesPath))
        return false;

    File file = sd->open(kEvolutionRulesPath, FILE_READ);
    if (!file)
        return false;

    char line[192] = {};
    while (readConfigLine(file, line, sizeof(line)) && speciesCount < maxSpecies)
    {
        char *content = trimField(line);
        if (content == nullptr || content[0] == '\0' || content[0] == '#')
            continue;

        char *speciesCode = nullptr;
        char *outfitCode = nullptr;
        char *sourceSpecies = nullptr;
        char *conditions = nullptr;
        if (!splitEvolutionRule(content, sourceSpecies, speciesCode, outfitCode, conditions))
            continue;

        bool duplicate = false;
        for (size_t i = 0; i < speciesCount; ++i)
        {
            if (strcmp(species[i], speciesCode) == 0)
            {
                duplicate = true;
                break;
            }
        }

        if (duplicate)
            continue;

        strncpy(species[speciesCount], speciesCode, 8);
        species[speciesCount][8] = '\0';
        ++speciesCount;
    }

    file.close();
    return speciesCount > 0;
}

bool SdAppearanceLoader::loadOutfits(const char *speciesCode, char outfits[][9], size_t maxOutfits, size_t &outfitCount)
{
    outfitCount = 0;
    if (sd == nullptr || outfits == nullptr || maxOutfits == 0)
        return false;

    char path[32] = {};
    if (!buildSpeciesOutfitListPath(path, sizeof(path), speciesCode))
        return false;

    File file = sd->open(path, FILE_READ);
    if (!file)
        return false;

    char line[128] = {};
    while (readConfigLine(file, line, sizeof(line)))
    {
        char *content = trimField(line);
        if (content == nullptr || content[0] == '\0' || content[0] == '#')
            continue;

        char *cursor = content;
        while (cursor != nullptr && *cursor != '\0' && outfitCount < maxOutfits)
        {
            char *sep = strchr(cursor, '|');
            if (sep != nullptr)
                *sep = '\0';

            char *outfitCode = trimField(cursor);
            if (isValidAppearanceCode(outfitCode))
            {
                strncpy(outfits[outfitCount], outfitCode, 8);
                outfits[outfitCount][8] = '\0';
                ++outfitCount;
            }

            cursor = (sep == nullptr) ? nullptr : sep + 1;
        }

        if (outfitCount > 0 || outfitCount >= maxOutfits)
            break;
    }

    file.close();
    return outfitCount > 0;
}

bool SdAppearanceLoader::findOutfitPreview(const char *speciesCode, const char *outfitCode, OutfitPreview &preview)
{
    preview = {};
    if (sd == nullptr || !isValidAppearanceCode(outfitCode))
        return false;

    char path[40] = {};
    if (!buildOutfitPreviewPath(path, sizeof(path), speciesCode))
        return false;

    File file = sd->open(path, FILE_READ);
    if (!file)
        return false;

    char line[192] = {};
    while (readConfigLine(file, line, sizeof(line)))
    {
        char *content = trimField(line);
        if (content == nullptr || content[0] == '\0' || content[0] == '#')
            continue;

        char *fields[6] = {};
        if (!splitOutfitPreviewRow(content, fields, 6))
            continue;

        if (strcmp(fields[0], outfitCode) != 0)
            continue;

        const uint16_t frameCount = static_cast<uint16_t>(strtoul(fields[1], nullptr, 10));
        const uint16_t frameIntervalMs = static_cast<uint16_t>(strtoul(fields[2], nullptr, 10));
        const uint16_t width = static_cast<uint16_t>(strtoul(fields[3], nullptr, 10));
        const uint16_t height = static_cast<uint16_t>(strtoul(fields[4], nullptr, 10));
        if (frameCount == 0 || width == 0 || height == 0)
            continue;

        if (!copyText(preview.outfitCode, sizeof(preview.outfitCode), fields[0]) ||
            !copyText(preview.path, sizeof(preview.path), fields[5]))
            continue;

        preview.frameCount = frameCount;
        preview.width = width;
        preview.height = height;
        preview.frameIntervalMs = frameIntervalMs;
        file.close();
        return true;
    }

    file.close();
    return false;
}
