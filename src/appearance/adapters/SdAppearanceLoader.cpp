#include "appearance/adapters/SdAppearanceLoader.h"

#include <stdio.h>
#include <string.h>
#include "appearance/adapters/EvolutionConditionContract.h"
#include "pet_behavior/domain/PetBehaviorStatSlot.h"
#include "shared/sd/SdTextRecordReader.h"
#include "shared/utils/TextBuffer.h"
#include "shared/utils/UnsignedDecimal.h"

namespace
{
constexpr const char *kEvolutionRulesPath = "/evolution_rules.txt";
constexpr size_t kMaxEvolutionRulesFileBytes = 8192;
constexpr size_t kMaxAppearanceIndexFileBytes = 8192;
constexpr size_t kMaxAppearanceLineBytes = 192;
constexpr int32_t kMinConditionValue = -2147483647L - 1L;
constexpr int32_t kMaxConditionValue = 2147483647L;

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

bool statValueBySource(
    const char *source,
    const PetStatSnapshot &stats,
    const ActivePetBehaviorStatSlots &activeSlots,
    int32_t &value)
{
    if (source == nullptr)
        return false;

    if (strcmp(source, "stage_days") == 0)
        value = static_cast<int32_t>(stats.stage_days);
    else
    {
        uint8_t slot = 0;
        if (!activeSlots.resolve(source, slot))
            return false;
        value = stats.customStats[slot];
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
                      const ActivePetBehaviorStatSlots &activeSlots)
{
    char *equals = strchr(condition, '=');
    if (equals == nullptr)
        return false;

    *equals = '\0';
    char *name = trimField(condition);
    char *rangeText = trimField(equals + 1);
    if (strcmp(name, "species") == 0 || strcmp(name, "outfit") == 0)
    {
        const char *actual = strcmp(name, "species") == 0 ? stats.species : stats.outfit;
        return isValidAppearanceCode(rangeText) && strcmp(actual, rangeText) == 0;
    }

    int32_t statValue = 0;
    int32_t minValue = 0;
    int32_t maxValue = 0;
    if (!statValueBySource(name, stats, activeSlots, statValue) ||
        !parseConditionRange(rangeText, minValue, maxValue))
    {
        return false;
    }

    return statValue >= minValue && statValue <= maxValue;
}

} // namespace

bool evaluateEvolutionConditions(
    char *conditions,
    const PetStatSnapshot &stats,
    const ActivePetBehaviorStatSlots &activeSlots)
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
            !conditionMatches(condition, stats, activeSlots))
        {
            return false;
        }

        cursor = (sep == nullptr) ? nullptr : sep + 1;
    }

    return true;
}

namespace
{

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

void SdAppearanceLoader::configureRuntimeContract(const PetBehaviorConfig &config)
{
    evolutionStatSlots.configure(config);
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

    struct Context
    {
        const PetStatSnapshot *stats;
        const ActivePetBehaviorStatSlots *activeSlots;
        AppearanceSelection *selection;
        bool found;
    } context = {&stats, &evolutionStatSlots, &selection, false};
    const bool loaded = loadEvolutionRecords(sd, [](void *rawContext, const SdTextRecord &record) {
        Context &context = *static_cast<Context *>(rawContext);
        if (isIgnoredEvolutionRecord(record))
            return SdTextRecordAction::Continue;
        EvolutionRule rule = {};
        if (!parseEvolutionRule(record, rule) ||
            !sourceMatches(rule.sourceSpecies, *context.stats) ||
            !evaluateEvolutionConditions(rule.conditions, *context.stats, *context.activeSlots))
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
            const uint16_t frameCount = static_cast<uint16_t>(parseUnsignedDecimalUnchecked(fields[1]));
            const uint16_t frameIntervalMs = static_cast<uint16_t>(parseUnsignedDecimalUnchecked(fields[2]));
            const uint16_t width = static_cast<uint16_t>(parseUnsignedDecimalUnchecked(fields[3]));
            const uint16_t height = static_cast<uint16_t>(parseUnsignedDecimalUnchecked(fields[4]));
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
