#include "appearance/adapters/SdAppearanceLoader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared/sd/SdTextRecordReader.h"
#include "shared/utils/TextBuffer.h"

namespace
{
constexpr const char *kEvolutionRulesPath = "/evolution_rules.txt";
constexpr size_t kMaxEvolutionRulesFileBytes = 8192;
constexpr size_t kMaxAppearanceIndexFileBytes = 8192;
constexpr size_t kMaxAppearanceLineBytes = 192;
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

void loadStateAliases(const PetBehaviorConfig &config, StateAlias aliases[], size_t maxAliases, size_t &aliasCount)
{
    aliasCount = 0;
    if (aliases == nullptr || maxAliases == 0)
        return;
    for (uint8_t slot = 0; slot < kPetBehaviorSlotCount && aliasCount < maxAliases; ++slot)
    {
        const PetBehaviorStatConfig &stat = config.stats[slot];
        if (!stat.active)
            continue;
        StateAlias &alias = aliases[aliasCount++];
        strncpy(alias.name, stat.name, sizeof(alias.name) - 1);
        alias.customIndex = slot;
        alias.minValue = stat.minValue;
        alias.maxValue = stat.maxValue;
        alias.defaultValue = stat.initialValue;
    }
}

bool statValueByName(const char *name, const PetStatSnapshot &stats, const StateAlias aliases[], size_t aliasCount, int32_t &value)
{
    if (name == nullptr)
        return false;

    if (strcmp(name, "stage_days") == 0)
        value = static_cast<int32_t>(stats.stage_days);
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

    if (strcmp(name, "species") == 0 || strcmp(name, "outfit") == 0)
    {
        const char *actual = strcmp(name, "species") == 0 ? stats.species : stats.outfit;
        return isValidAppearanceCode(rangeText) && strcmp(actual, rangeText) == 0;
    }

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

bool conditionIsValid(char *condition, const StateAlias aliases[], size_t aliasCount)
{
    char *equals = strchr(condition, '=');
    if (equals == nullptr)
        return false;
    *equals = '\0';
    char *name = trimField(condition);
    char *value = trimField(equals + 1);
    if (!isValidStatName(name))
        return false;
    if (strcmp(name, "species") == 0 || strcmp(name, "outfit") == 0)
        return isValidAppearanceCode(value);
    if (strcmp(name, "stage_days") != 0)
    {
        bool found = false;
        for (size_t index = 0; index < aliasCount; ++index)
            found = found || strcmp(name, aliases[index].name) == 0;
        if (!found)
            return false;
    }
    int32_t minimum = 0;
    int32_t maximum = 0;
    return parseConditionRange(value, minimum, maximum);
}

bool conditionsAreValid(char *conditions, const StateAlias aliases[], size_t aliasCount)
{
    conditions = trimField(conditions);
    if (conditions == nullptr)
        return false;
    if (conditions[0] == '\0')
        return true;
    char *cursor = conditions;
    while (cursor != nullptr && *cursor != '\0')
    {
        char *separator = strchr(cursor, ',');
        if (separator != nullptr)
            *separator = '\0';
        char *condition = trimField(cursor);
        if (condition == nullptr || condition[0] == '\0' ||
            !conditionIsValid(condition, aliases, aliasCount))
            return false;
        cursor = separator == nullptr ? nullptr : separator + 1;
    }
    return true;
}

struct EvolutionRule
{
    char *sourceSpecies;
    char *speciesCode;
    char *outfitCode;
    char *conditions;
};

bool parseEvolutionRule(const SdTextRecord &record, EvolutionRule &rule)
{
    rule = {};
    if (record.fieldOverflow || record.fieldCount != 4)
        return false;

    rule.sourceSpecies = trimField(record.fields[0]);
    rule.speciesCode = trimField(record.fields[1]);
    rule.outfitCode = trimField(record.fields[2]);
    rule.conditions = trimField(record.fields[3]);
    return (strcmp(rule.sourceSpecies, "*") == 0 || isValidAppearanceCode(rule.sourceSpecies)) &&
           isValidAppearanceCode(rule.speciesCode) &&
           isValidAppearanceCode(rule.outfitCode);
}

bool isIgnoredEvolutionRecord(const SdTextRecord &record)
{
    if (record.fieldCount == 0)
        return true;
    char *content = trimField(record.fields[0]);
    return content == nullptr || content[0] == '\0' || content[0] == '#';
}

bool loadEvolutionRecords(SdFat *sd, SdDelimitedTextRecordHandler handler, void *context)
{
    return loadSdDelimitedTextRecords(sd, kEvolutionRulesPath, kMaxEvolutionRulesFileBytes,
                                      kMaxAppearanceLineBytes, handler, context);
}

bool buildSpeciesOutfitListPath(char *dest, size_t destSize, const char *speciesCode)
{
    if (dest == nullptr || destSize == 0 || !isValidAppearanceCode(speciesCode))
        return false;

    TextBuffer path(dest, destSize);
    return path.append("/index/") && path.append(speciesCode) && path.append(".txt") && path.ok();
}

bool evolutionSpeciesExists(SdFat *sd, const char *expectedCode)
{
    if (sd == nullptr || !isValidAppearanceCode(expectedCode))
        return false;
    struct Context
    {
        const char *expectedCode;
        bool found;
    } context = {expectedCode, false};
    const bool loaded = loadEvolutionRecords(sd, [](void *rawContext, const SdTextRecord &record) {
        Context &context = *static_cast<Context *>(rawContext);
        if (isIgnoredEvolutionRecord(record))
            return SdTextRecordAction::Continue;
        EvolutionRule rule = {};
        context.found = parseEvolutionRule(record, rule) && strcmp(rule.speciesCode, context.expectedCode) == 0;
        return context.found ? SdTextRecordAction::Stop : SdTextRecordAction::Continue;
    }, &context);
    return loaded && context.found;
}

bool outfitExists(SdFat *sd, const char *speciesCode, const char *expectedCode)
{
    char path[32] = {};
    if (sd == nullptr || !isValidAppearanceCode(expectedCode) ||
        !buildSpeciesOutfitListPath(path, sizeof(path), speciesCode))
        return false;
    struct Context
    {
        const char *expectedCode;
        bool found;
    } context = {expectedCode, false};
    const bool loaded = loadSdDelimitedTextRecords(sd, path, kMaxAppearanceIndexFileBytes, 128,
        [](void *rawContext, const SdTextRecord &record) {
            Context &context = *static_cast<Context *>(rawContext);
            for (uint8_t index = 0; index < record.fieldCount; ++index)
            {
                context.found = strcmp(trimField(record.fields[index]), context.expectedCode) == 0;
                if (context.found)
                    return SdTextRecordAction::Stop;
            }
            return SdTextRecordAction::Continue;
        }, &context);
    return loaded && context.found;
}

bool conditionReferencesExist(char *conditions, SdFat *sd, const char *sourceSpecies)
{
    char *cursor = trimField(conditions);
    while (cursor != nullptr && cursor[0] != '\0')
    {
        char *separator = strchr(cursor, ',');
        if (separator != nullptr)
            *separator = '\0';
        char *equals = strchr(cursor, '=');
        if (equals == nullptr)
            return false;
        *equals = '\0';
        const char *name = trimField(cursor);
        const char *value = trimField(equals + 1);
        if (strcmp(name, "species") == 0 && !evolutionSpeciesExists(sd, value))
            return false;
        if (strcmp(name, "outfit") == 0 && !outfitExists(sd, sourceSpecies, value))
            return false;
        cursor = separator == nullptr ? nullptr : separator + 1;
    }
    return true;
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

} // namespace

SdAppearanceLoader::SdAppearanceLoader(SdFat *refSd) : sd(refSd)
{
}

bool SdAppearanceLoader::validateEvolutionContract(const PetBehaviorConfig &config)
{
    evolutionContractValidated = false;
    evolutionStatCount = 0;
    if (sd == nullptr || !sd->exists(kEvolutionRulesPath))
        return false;

    StateAlias aliases[kMaxStateAliases] = {};
    size_t aliasCount = 0;
    loadStateAliases(config, aliases, kMaxStateAliases, aliasCount);
    for (size_t index = 0; index < aliasCount; ++index)
    {
        strncpy(evolutionStats[index].name, aliases[index].name, kPetBehaviorStatNameSize - 1);
        evolutionStats[index].slot = aliases[index].customIndex;
    }
    evolutionStatCount = static_cast<uint8_t>(aliasCount);
    struct ValidationContext
    {
        StateAlias *aliases;
        size_t aliasCount;
        bool foundInitial;
    } validation = {aliases, aliasCount, false};
    bool valid = loadEvolutionRecords(sd, [](void *rawContext, const SdTextRecord &record) {
        ValidationContext &context = *static_cast<ValidationContext *>(rawContext);
        if (isIgnoredEvolutionRecord(record))
            return SdTextRecordAction::Continue;
        EvolutionRule rule = {};
        if (!parseEvolutionRule(record, rule))
            return SdTextRecordAction::Error;
        if (strcmp(rule.sourceSpecies, "init") == 0)
        {
            const bool validInitial = !context.foundInitial && rule.conditions[0] == '\0';
            context.foundInitial = true;
            return validInitial ? SdTextRecordAction::Continue : SdTextRecordAction::Error;
        }
        return conditionsAreValid(rule.conditions, context.aliases, context.aliasCount)
                   ? SdTextRecordAction::Continue
                   : SdTextRecordAction::Error;
    }, &validation);
    valid = valid && validation.foundInitial;
    if (valid)
    {
        struct ReferenceContext
        {
            SdFat *sd;
        } references = {sd};
        valid = loadEvolutionRecords(sd, [](void *rawContext, const SdTextRecord &record) {
            ReferenceContext &context = *static_cast<ReferenceContext *>(rawContext);
            if (isIgnoredEvolutionRecord(record))
                return SdTextRecordAction::Continue;
            EvolutionRule rule = {};
            const bool recordValid = parseEvolutionRule(record, rule) &&
                                     outfitExists(context.sd, rule.speciesCode, rule.outfitCode) &&
                                     (strcmp(rule.sourceSpecies, "init") == 0 ||
                                      (evolutionSpeciesExists(context.sd, rule.sourceSpecies) &&
                                       conditionReferencesExist(rule.conditions, context.sd, rule.sourceSpecies)));
            return recordValid ? SdTextRecordAction::Continue : SdTextRecordAction::Error;
        }, &references);
    }
    evolutionContractValidated = valid;
    return evolutionContractValidated;
}

bool SdAppearanceLoader::findInitialAppearance(AppearanceSelection &selection)
{
    selection = {};
    if (sd == nullptr || !sd->exists(kEvolutionRulesPath))
        return false;

    struct Context
    {
        AppearanceSelection *selection;
        bool found;
    } context = {&selection, false};
    const bool loaded = loadEvolutionRecords(sd, [](void *rawContext, const SdTextRecord &record) {
        Context &context = *static_cast<Context *>(rawContext);
        if (isIgnoredEvolutionRecord(record))
            return SdTextRecordAction::Continue;
        EvolutionRule rule = {};
        if (!parseEvolutionRule(record, rule))
            return SdTextRecordAction::Continue;
        if (strcmp(rule.sourceSpecies, "init") != 0)
            return SdTextRecordAction::Continue;
        strncpy(context.selection->speciesCode, rule.speciesCode, sizeof(context.selection->speciesCode) - 1);
        context.selection->speciesCode[sizeof(context.selection->speciesCode) - 1] = '\0';
        strncpy(context.selection->outfitCode, rule.outfitCode, sizeof(context.selection->outfitCode) - 1);
        context.selection->outfitCode[sizeof(context.selection->outfitCode) - 1] = '\0';
        context.found = true;
        return SdTextRecordAction::Stop;
    }, &context);
    return loaded && context.found;
}

bool SdAppearanceLoader::findEvolutionTarget(const PetStatSnapshot &stats, AppearanceSelection &selection)
{
    selection = {};
    if (sd == nullptr || !sd->exists(kEvolutionRulesPath))
        return false;

    StateAlias aliases[kMaxStateAliases] = {};
    const size_t aliasCount = evolutionStatCount;
    if (!evolutionContractValidated)
        return false;
    for (size_t index = 0; index < aliasCount; ++index)
    {
        strncpy(aliases[index].name, evolutionStats[index].name, sizeof(aliases[index].name) - 1);
        aliases[index].customIndex = evolutionStats[index].slot;
    }

    struct Context
    {
        const PetStatSnapshot *stats;
        const StateAlias *aliases;
        size_t aliasCount;
        AppearanceSelection *selection;
        bool found;
    } context = {&stats, aliases, aliasCount, &selection, false};
    const bool loaded = loadEvolutionRecords(sd, [](void *rawContext, const SdTextRecord &record) {
        Context &context = *static_cast<Context *>(rawContext);
        if (isIgnoredEvolutionRecord(record))
            return SdTextRecordAction::Continue;
        EvolutionRule rule = {};
        if (!parseEvolutionRule(record, rule) ||
            !sourceMatches(rule.sourceSpecies, *context.stats) ||
            !conditionsMatch(rule.conditions, *context.stats, context.aliases, context.aliasCount))
            return SdTextRecordAction::Continue;
        strncpy(context.selection->speciesCode, rule.speciesCode, sizeof(context.selection->speciesCode) - 1);
        context.selection->speciesCode[sizeof(context.selection->speciesCode) - 1] = '\0';
        strncpy(context.selection->outfitCode, rule.outfitCode, sizeof(context.selection->outfitCode) - 1);
        context.selection->outfitCode[sizeof(context.selection->outfitCode) - 1] = '\0';
        context.found = true;
        return SdTextRecordAction::Stop;
    }, &context);
    return loaded && context.found;
}

bool SdAppearanceLoader::loadSpecies(char species[][9], size_t maxSpecies, size_t &speciesCount)
{
    speciesCount = 0;
    if (sd == nullptr || species == nullptr || maxSpecies == 0 || !sd->exists(kEvolutionRulesPath))
        return false;

    struct Context
    {
        char (*species)[9];
        size_t maxSpecies;
        size_t *speciesCount;
    } context = {species, maxSpecies, &speciesCount};
    const bool loaded = loadEvolutionRecords(sd, [](void *rawContext, const SdTextRecord &record) {
        Context &context = *static_cast<Context *>(rawContext);
        if (isIgnoredEvolutionRecord(record))
            return SdTextRecordAction::Continue;
        EvolutionRule rule = {};
        if (!parseEvolutionRule(record, rule))
            return SdTextRecordAction::Continue;

        bool duplicate = false;
        for (size_t i = 0; i < *context.speciesCount; ++i)
        {
            if (strcmp(context.species[i], rule.speciesCode) == 0)
            {
                duplicate = true;
                break;
            }
        }

        if (duplicate)
            return SdTextRecordAction::Continue;
        strncpy(context.species[*context.speciesCount], rule.speciesCode, 8);
        context.species[*context.speciesCount][8] = '\0';
        ++*context.speciesCount;
        return *context.speciesCount >= context.maxSpecies ? SdTextRecordAction::Stop
                                                           : SdTextRecordAction::Continue;
    }, &context);
    return loaded && speciesCount > 0;
}

bool SdAppearanceLoader::loadOutfits(const char *speciesCode, char outfits[][9], size_t maxOutfits, size_t &outfitCount)
{
    outfitCount = 0;
    if (sd == nullptr || outfits == nullptr || maxOutfits == 0)
        return false;

    char path[32] = {};
    if (!buildSpeciesOutfitListPath(path, sizeof(path), speciesCode))
        return false;

    struct Context
    {
        char (*outfits)[9];
        size_t maxOutfits;
        size_t *outfitCount;
    } context = {outfits, maxOutfits, &outfitCount};
    const bool loaded = loadSdDelimitedTextRecords(sd, path, kMaxAppearanceIndexFileBytes, 128,
        [](void *rawContext, const SdTextRecord &record) {
            Context &context = *static_cast<Context *>(rawContext);
            if (record.fieldCount == 0)
                return SdTextRecordAction::Continue;
            char *first = trimField(record.fields[0]);
            if (first == nullptr || first[0] == '\0' || first[0] == '#')
                return SdTextRecordAction::Continue;
            for (uint8_t index = 0; index < record.fieldCount && *context.outfitCount < context.maxOutfits; ++index)
            {
                char *outfitCode = trimField(record.fields[index]);
                if (!isValidAppearanceCode(outfitCode))
                    continue;
                strncpy(context.outfits[*context.outfitCount], outfitCode, 8);
                context.outfits[*context.outfitCount][8] = '\0';
                ++*context.outfitCount;
            }
            return *context.outfitCount > 0 ? SdTextRecordAction::Stop : SdTextRecordAction::Continue;
        }, &context);
    return loaded && outfitCount > 0;
}

bool SdAppearanceLoader::findOutfitPreview(const char *speciesCode, const char *outfitCode, OutfitPreview &preview)
{
    preview = {};
    if (sd == nullptr || !isValidAppearanceCode(outfitCode))
        return false;

    char path[40] = {};
    if (!buildOutfitPreviewPath(path, sizeof(path), speciesCode))
        return false;

    struct Context
    {
        const char *outfitCode;
        OutfitPreview *preview;
        bool found;
    } context = {outfitCode, &preview, false};
    const bool loaded = loadSdDelimitedTextRecords(sd, path, kMaxAppearanceIndexFileBytes,
        kMaxAppearanceLineBytes, [](void *rawContext, const SdTextRecord &record) {
            Context &context = *static_cast<Context *>(rawContext);
            if (record.fieldOverflow || record.fieldCount != 6)
                return SdTextRecordAction::Continue;
            char *fields[6] = {};
            for (uint8_t index = 0; index < record.fieldCount; ++index)
            {
                fields[index] = trimField(record.fields[index]);
                if (fields[index] == nullptr || fields[index][0] == '\0')
                    return SdTextRecordAction::Continue;
            }
            if (fields[0][0] == '#' || strcmp(fields[0], context.outfitCode) != 0)
                return SdTextRecordAction::Continue;
            const uint16_t frameCount = static_cast<uint16_t>(strtoul(fields[1], nullptr, 10));
            const uint16_t frameIntervalMs = static_cast<uint16_t>(strtoul(fields[2], nullptr, 10));
            const uint16_t width = static_cast<uint16_t>(strtoul(fields[3], nullptr, 10));
            const uint16_t height = static_cast<uint16_t>(strtoul(fields[4], nullptr, 10));
            if (frameCount == 0 || width == 0 || height == 0 ||
                !copyText(context.preview->outfitCode, sizeof(context.preview->outfitCode), fields[0]) ||
                !copyText(context.preview->path, sizeof(context.preview->path), fields[5]))
                return SdTextRecordAction::Continue;
            context.preview->frameCount = frameCount;
            context.preview->width = width;
            context.preview->height = height;
            context.preview->frameIntervalMs = frameIntervalMs;
            context.found = true;
            return SdTextRecordAction::Stop;
        }, &context);
    return loaded && context.found;
}
