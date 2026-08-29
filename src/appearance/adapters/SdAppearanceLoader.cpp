#include "appearance/adapters/SdAppearanceLoader.h"

#include <stdio.h>
#include <string.h>
#include "appearance/adapters/EvolutionConditionContract.h"
#include "pet_behavior/domain/PetBehaviorStatSlot.h"
#include "shared/sd/SdTextRecordReader.h"
#include "shared/utils/CanonicalDecimal.h"
#include "shared/utils/TextBuffer.h"


namespace
{
constexpr const char *kAppearanceContractPathV1 = "/appearance_contract.txt";
constexpr const char *kEvolutionContractPathV2 = "/evolution_rules.txt";
constexpr size_t kMaxAppearanceContractBytesV1 = 16384;
constexpr size_t kMaxEvolutionContractBytesV2 = 16384;

bool parseCanonicalSlotV1(const char *text, uint8_t &slot)
{
    uint32_t parsed = 0;
    if (!CanonicalDecimal::parseUnsigned(text, UINT8_MAX, parsed, false))
        return false;
    slot = static_cast<uint8_t>(parsed);
    return true;
}

bool parseSignedV1(const char *text, int32_t &value)
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
    uint32_t magnitude = 0;
    for (; *cursor != '\0'; ++cursor)
    {
        if (*cursor < '0' || *cursor > '9')
            return false;
        const uint32_t digit = static_cast<uint32_t>(*cursor - '0');
        const uint32_t maximum = negative ? 2147483648UL : 2147483647UL;
        if (magnitude > (maximum - digit) / 10U)
            return false;
        magnitude = magnitude * 10U + digit;
    }
    value = negative ? static_cast<int32_t>(-static_cast<int64_t>(magnitude))
                     : static_cast<int32_t>(magnitude);
    return true;
}

bool decodeEnvelopeV1(const SdTextRecord &record,
                      const AssetData::RuntimeManifest &manifest,
                      bool &assetDataSeen,
                      bool &bundleSeen)
{
    if (strcmp(record.fields[0], "asset_data") == 0)
    {
        if (assetDataSeen || record.fieldCount != 2 || strcmp(record.fields[1], "1") != 0)
            return false;
        assetDataSeen = true;
        return true;
    }
    if (strcmp(record.fields[0], "bundle_id") == 0)
    {
        AssetData::BundleId bundleId = {};
        if (!assetDataSeen || bundleSeen || record.fieldCount != 2 ||
            !AssetData::parseBundleId(record.fields[1], bundleId) ||
            !AssetData::sameBundleId(bundleId, manifest.bundleId))
            return false;
        bundleSeen = true;
        return true;
    }
    return false;
}

struct AppearanceQueryV1
{
    const AssetData::RuntimeManifest *manifest;
    BundleReader *bundleReader;
    AppearanceSelection *initial = nullptr;
    OutfitPreview *preview = nullptr;
    uint8_t *species = nullptr;
    uint8_t *outfits = nullptr;
    size_t capacity = 0;
    size_t count = 0;
    uint8_t filterSpecies = 0;
    uint8_t filterOutfit = 0;
    bool assetDataSeen = false;
    bool bundleSeen = false;
    bool initialSeen = false;
    bool found = false;
};

bool decodeAppearanceV1(void *rawContext, const SdTextRecord &record)
{
    if (rawContext == nullptr || record.fieldOverflow || record.fieldCount == 0)
        return false;
    AppearanceQueryV1 &context = *static_cast<AppearanceQueryV1 *>(rawContext);
    if (strcmp(record.fields[0], "asset_data") == 0 || strcmp(record.fields[0], "bundle_id") == 0)
        return decodeEnvelopeV1(record, *context.manifest, context.assetDataSeen, context.bundleSeen);
    if (!context.assetDataSeen || !context.bundleSeen)
        return false;
    if (strcmp(record.fields[0], "initial") == 0)
    {
        uint8_t speciesSlot = 0;
        uint8_t outfitSlot = 0;
        if (context.initialSeen || record.fieldCount != 3 ||
            !parseCanonicalSlotV1(record.fields[1], speciesSlot) ||
            !parseCanonicalSlotV1(record.fields[2], outfitSlot))
            return false;
        context.initialSeen = true;
        if (context.initial != nullptr)
        {
            context.initial->speciesSlot = speciesSlot;
            context.initial->outfitSlot = outfitSlot;
        }
        return true;
    }
    if (strcmp(record.fields[0], "species") == 0)
    {
        uint8_t speciesSlot = 0;
        uint8_t entryOutfitSlot = 0;
        if (record.fieldCount != 3 || !parseCanonicalSlotV1(record.fields[1], speciesSlot) ||
            !parseCanonicalSlotV1(record.fields[2], entryOutfitSlot))
            return false;
        if (context.species != nullptr && context.count < context.capacity)
            context.species[context.count++] = speciesSlot;
        if (context.outfits != nullptr && speciesSlot == context.filterSpecies &&
            context.count < context.capacity)
            context.outfits[context.count++] = entryOutfitSlot;
        return true;
    }
    if (strcmp(record.fields[0], "outfit") == 0)
    {
        uint8_t speciesSlot = 0;
        uint8_t outfitSlot = 0;
        AssetData::AnimationRef reference = {};
        if (record.fieldCount != 5 || !parseCanonicalSlotV1(record.fields[1], speciesSlot) ||
            !parseCanonicalSlotV1(record.fields[2], outfitSlot) ||
            !AssetData::parseAnimationRef(record.fields[3], record.fields[4],
                                          *context.manifest, reference) ||
            reference.speciesSlot != speciesSlot || reference.outfitSlot != outfitSlot ||
            context.bundleReader == nullptr ||
            !AssetData::animationReferenceExists(*context.bundleReader, reference))
            return false;
        if (context.outfits != nullptr && speciesSlot == context.filterSpecies)
        {
            bool duplicate = false;
            for (size_t index = 0; index < context.count; ++index)
                duplicate |= context.outfits[index] == outfitSlot;
            if (!duplicate && context.count < context.capacity)
                context.outfits[context.count++] = outfitSlot;
        }
        if (context.preview != nullptr && speciesSlot == context.filterSpecies &&
            outfitSlot == context.filterOutfit)
        {
            context.preview->speciesSlot = speciesSlot;
            context.preview->outfitSlot = outfitSlot;
            context.preview->animation = reference;
            context.found = true;
        }
        if (context.initial != nullptr && context.initialSeen &&
            speciesSlot == context.initial->speciesSlot && outfitSlot == context.initial->outfitSlot)
            context.found = true;
        return true;
    }
    return false;
}

bool loadAppearanceQueryV1(SdFat *sd, AppearanceQueryV1 &query)
{
    return loadSdTextRecords(sd, kAppearanceContractPathV1, kMaxAppearanceContractBytesV1,
                             "appearance_contract", "1", decodeAppearanceV1, &query) &&
           query.assetDataSeen && query.bundleSeen && query.initialSeen;
}

bool parseSlotPairV1(const char *text, uint8_t &speciesSlot, uint8_t &outfitSlot)
{
    if (text == nullptr)
        return false;
    const char *separator = strchr(text, ':');
    if (separator == nullptr || strchr(separator + 1, ':') != nullptr)
        return false;
    char species[4] = {};
    const size_t length = static_cast<size_t>(separator - text);
    if (length == 0 || length >= sizeof(species))
        return false;
    memcpy(species, text, length);
    return parseCanonicalSlotV1(species, speciesSlot) &&
           parseCanonicalSlotV1(separator + 1, outfitSlot);
}

enum class ConditionMatchV2 : uint8_t
{
    Invalid,
    NoMatch,
    Match,
};

ConditionMatchV2 conditionMatchesV2(char *condition,
                                    const PetStatSnapshot &stats,
                                    const ActivePetBehaviorStatSlots &activeSlots)
{
    char *equals = strchr(condition, '=');
    if (equals == nullptr)
        return ConditionMatchV2::Invalid;
    *equals = '\0';
    const char *source = condition;
    char *range = equals + 1;
    if (strcmp(source, "species") == 0)
    {
        uint8_t slot = 0;
        if (!parseCanonicalSlotV1(range, slot))
            return ConditionMatchV2::Invalid;
        return slot == stats.speciesSlot ? ConditionMatchV2::Match : ConditionMatchV2::NoMatch;
    }
    if (strcmp(source, "outfit") == 0)
    {
        uint8_t speciesSlot = 0;
        uint8_t outfitSlot = 0;
        if (!parseSlotPairV1(range, speciesSlot, outfitSlot))
            return ConditionMatchV2::Invalid;
        return speciesSlot == stats.speciesSlot && outfitSlot == stats.outfitSlot
                   ? ConditionMatchV2::Match
                   : ConditionMatchV2::NoMatch;
    }
    int32_t current = 0;
    if (strcmp(source, "stage_days") == 0)
        current = static_cast<int32_t>(stats.stage_days);
    else
    {
        uint8_t slot = 0;
        if (!activeSlots.resolve(source, slot))
            return ConditionMatchV2::Invalid;
        current = stats.customStats[slot];
    }
    int32_t minimum = INT32_MIN;
    int32_t maximum = INT32_MAX;
    char *separator = strstr(range, "..");
    if (separator == nullptr)
    {
        if (!parseSignedV1(range, minimum))
            return ConditionMatchV2::Invalid;
        maximum = minimum;
    }
    else
    {
        *separator = '\0';
        if (strcmp(range, "*") != 0 && !parseSignedV1(range, minimum))
            return ConditionMatchV2::Invalid;
        if (strcmp(separator + 2, "*") != 0 && !parseSignedV1(separator + 2, maximum))
            return ConditionMatchV2::Invalid;
    }
    if (minimum > maximum)
        return ConditionMatchV2::Invalid;
    return minimum <= current && current <= maximum
               ? ConditionMatchV2::Match
               : ConditionMatchV2::NoMatch;
}

ConditionMatchV2 conditionsMatchV2(char *conditions,
                                   const PetStatSnapshot &stats,
                                   const ActivePetBehaviorStatSlots &activeSlots)
{
    if (conditions == nullptr || conditions[0] == '\0')
        return ConditionMatchV2::Match;
    bool allMatch = true;
    char *cursor = conditions;
    while (cursor != nullptr)
    {
        char *separator = strchr(cursor, ',');
        if (separator != nullptr)
            *separator = '\0';
        const ConditionMatchV2 result = conditionMatchesV2(cursor, stats, activeSlots);
        if (result == ConditionMatchV2::Invalid)
            return result;
        allMatch &= result == ConditionMatchV2::Match;
        cursor = separator == nullptr ? nullptr : separator + 1;
    }
    return allMatch ? ConditionMatchV2::Match : ConditionMatchV2::NoMatch;
}

struct EvolutionQueryV2
{
    const AssetData::RuntimeManifest *manifest;
    BundleReader *bundleReader;
    const PetStatSnapshot *stats;
    const ActivePetBehaviorStatSlots *activeSlots;
    AppearanceSelection *selection;
    char selectedRelationship[24] = {};
    bool assetDataSeen = false;
    bool bundleSeen = false;
    bool selected = false;
};

bool decodeEvolutionV2(void *rawContext, const SdTextRecord &record)
{
    if (rawContext == nullptr || record.fieldOverflow || record.fieldCount == 0)
        return false;
    EvolutionQueryV2 &context = *static_cast<EvolutionQueryV2 *>(rawContext);
    if (strcmp(record.fields[0], "asset_data") == 0 || strcmp(record.fields[0], "bundle_id") == 0)
        return decodeEnvelopeV1(record, *context.manifest, context.assetDataSeen, context.bundleSeen);
    if (!context.assetDataSeen || !context.bundleSeen)
        return false;
    if (strcmp(record.fields[0], "relationship") == 0)
    {
        uint8_t sourceSpecies = 0;
        uint8_t targetSpecies = 0;
        uint8_t targetOutfit = 0;
        if (record.fieldCount != 6 ||
            !parseCanonicalSlotV1(record.fields[2], sourceSpecies) ||
            !parseCanonicalSlotV1(record.fields[3], targetSpecies) ||
            !parseCanonicalSlotV1(record.fields[4], targetOutfit))
            return false;
        const uint8_t currentSpecies = context.stats->speciesSlot;
        const ConditionMatchV2 conditions =
            conditionsMatchV2(record.fields[5], *context.stats, *context.activeSlots);
        if (conditions == ConditionMatchV2::Invalid)
            return false;
        if (context.selected || sourceSpecies != currentSpecies ||
            conditions == ConditionMatchV2::NoMatch)
            return true;
        if (strlen(record.fields[1]) >= sizeof(context.selectedRelationship))
            return false;
        strcpy(context.selectedRelationship, record.fields[1]);
        if (context.selection != nullptr)
        {
            context.selection->speciesSlot = targetSpecies;
            context.selection->outfitSlot = targetOutfit;
        }
        context.selected = true;
        return true;
    }
    if (strcmp(record.fields[0], "evolution_animation") == 0)
    {
        AssetData::AnimationRef reference = {};
        if (record.fieldCount != 4 ||
            !AssetData::parseAnimationRef(record.fields[2], record.fields[3],
                                          *context.manifest, reference) ||
            context.bundleReader == nullptr ||
            !AssetData::animationReferenceExists(*context.bundleReader, reference))
            return false;
        if (context.selection != nullptr && context.selected &&
            strcmp(record.fields[1], context.selectedRelationship) == 0 &&
            reference.speciesSlot == context.stats->speciesSlot &&
            reference.outfitSlot == context.stats->outfitSlot)
            context.selection->evolutionAnimation = reference;
        return true;
    }
    return false;
}

bool loadEvolutionQueryV2(SdFat *sd, EvolutionQueryV2 &query)
{
    return loadSdTextRecords(sd, kEvolutionContractPathV2, kMaxEvolutionContractBytesV2,
                             "evolution_rules", "2", decodeEvolutionV2, &query) &&
           query.assetDataSeen && query.bundleSeen;
}
} // namespace

SdAppearanceLoader::SdAppearanceLoader(SdFat *refSd)
    : sd(refSd), bundleReader(refSd, verificationScratch, sizeof(verificationScratch))
{
}

void SdAppearanceLoader::configureRuntimeContract(const PetBehaviorConfig &config)
{
    evolutionStatSlots.configure(config);
    assetManifest = config.assetManifest;
    bundleReader.configureBundle(assetManifest.bundleId);
    lastContractSucceeded = true;
    contractErrorResource[0] = '\0';
}

bool SdAppearanceLoader::validateRuntimeContracts(const PetStatSnapshot &stats)
{
    AppearanceQueryV1 appearance = {&assetManifest, &bundleReader};
    const bool appearanceValid = loadAppearanceQueryV1(sd, appearance);
    EvolutionQueryV2 evolution = {&assetManifest, &bundleReader, &stats, &evolutionStatSlots, nullptr};
    const bool evolutionValid = appearanceValid && loadEvolutionQueryV2(sd, evolution);
    lastContractSucceeded = appearanceValid && evolutionValid;
    if (!lastContractSucceeded)
    {
        const char *pack = bundleReader.firstErrorResource();
        const char *resource = pack != nullptr && pack[0] != '\0'
                                   ? pack
                                   : (appearanceValid ? "evolution_rules" : "appearance_contract");
        strncpy(contractErrorResource, resource, sizeof(contractErrorResource) - 1);
        contractErrorResource[sizeof(contractErrorResource) - 1] = '\0';
    }
    return lastContractSucceeded;
}

bool SdAppearanceLoader::lastContractLoadSucceeded() const
{
    return lastContractSucceeded;
}

const char *SdAppearanceLoader::firstAssetDataErrorResource() const
{
    const char *pack = bundleReader.firstErrorResource();
    return pack != nullptr && pack[0] != '\0' ? pack : contractErrorResource;
}

bool SdAppearanceLoader::findInitialAppearance(AppearanceSelection &selection)
{
    selection = {};
    AppearanceQueryV1 query = {&assetManifest, &bundleReader};
    query.initial = &selection;
    return loadAppearanceQueryV1(sd, query) && query.found;
}

bool SdAppearanceLoader::findEvolutionTarget(const PetStatSnapshot &stats, AppearanceSelection &selection)
{
    selection = {};
    EvolutionQueryV2 query = {&assetManifest, &bundleReader, &stats, &evolutionStatSlots, &selection};
    lastContractSucceeded = loadEvolutionQueryV2(sd, query);
    if (!lastContractSucceeded)
    {
        const char *pack = bundleReader.firstErrorResource();
        const char *resource = pack != nullptr && pack[0] != '\0' ? pack : "evolution_rules";
        strncpy(contractErrorResource, resource, sizeof(contractErrorResource) - 1);
        contractErrorResource[sizeof(contractErrorResource) - 1] = '\0';
    }
    return lastContractSucceeded && query.selected;
}

bool SdAppearanceLoader::loadSpecies(uint8_t *species, size_t maxSpecies, size_t &speciesCount)
{
    speciesCount = 0;
    if (species == nullptr || maxSpecies == 0)
        return false;
    AppearanceQueryV1 query = {&assetManifest, &bundleReader};
    query.species = species;
    query.capacity = maxSpecies;
    const bool loaded = loadAppearanceQueryV1(sd, query);
    speciesCount = query.count;
    return loaded && speciesCount > 0;
}

bool SdAppearanceLoader::loadOutfits(uint8_t speciesSlot, uint8_t *outfits,
                                     size_t maxOutfits, size_t &outfitCount)
{
    outfitCount = 0;
    if (speciesSlot == 0 || outfits == nullptr || maxOutfits == 0)
        return false;
    AppearanceQueryV1 query = {&assetManifest, &bundleReader};
    query.outfits = outfits;
    query.capacity = maxOutfits;
    query.filterSpecies = speciesSlot;
    const bool loaded = loadAppearanceQueryV1(sd, query);
    outfitCount = query.count;
    return loaded && outfitCount > 0;
}

bool SdAppearanceLoader::findOutfitPreview(uint8_t speciesSlot, uint8_t outfitSlot,
                                           OutfitPreview &preview)
{
    preview = {};
    if (speciesSlot == 0 || outfitSlot == 0)
        return false;
    AppearanceQueryV1 query = {&assetManifest, &bundleReader};
    query.preview = &preview;
    query.filterSpecies = speciesSlot;
    query.filterOutfit = outfitSlot;
    return loadAppearanceQueryV1(sd, query) && query.found;
}
