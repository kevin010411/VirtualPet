#include "pet_behavior/domain/RuntimeTableBehavior.h"

#include <limits.h>
#include <string.h>
#include "appearance/domain/RuntimeTableAppearance.h"
#include "commands/domain/SystemCommandCatalog.h"
#include "shared/sd/SdBinaryRead.h"

namespace
{
constexpr char kRuntimeTablePath[] = "/runtime.bin";
constexpr uint8_t kMagic[4] = {'V', 'P', 'R', 'T'};
constexpr uint16_t kVersion = 1;
constexpr uint16_t kHeaderSize = 64;
constexpr uint16_t kSectionEntrySize = 16;
constexpr uint16_t kMaxSections = 32;
constexpr uint32_t kMaxFileSize = 16777216UL;
constexpr uint16_t kNone16 = 0xffffU;
constexpr uint32_t kPetBehaviorFeature = 1UL << 0;
constexpr uint32_t kStatusFeature = 1UL << 1;
constexpr uint32_t kGuessGameFeature = 1UL << 4;
constexpr uint32_t kPredictFeature = 1UL << 5;
constexpr uint32_t kStartupAnimationFeature = 1UL << 6;
constexpr uint32_t kFirstStartAnimationFeature = 1UL << 7;
constexpr uint32_t kDynamicActionLayoutFeature = 1UL << 8;
constexpr uint32_t kSequentialStatusFeature = 1UL << 9;
constexpr uint32_t kFirstLaunchSelectionFeature = 1UL << 10;
constexpr uint32_t kOutfitChooseAnimationFeature = 1UL << 11;
constexpr uint32_t kKnownFeatures = (1UL << 12) - 1UL;

#ifndef ENABLE_GUESS_GAME_SINGLE_ROUND
#define ENABLE_GUESS_GAME_SINGLE_ROUND 0
#endif

enum SectionType : uint16_t
{
    Profile = 1,
    Persistence = 2,
    AssetRefs = 3,
    Animations = 4,
    PetStats = 10,
    IdleTriggers = 11,
    Actions = 12,
    ActionOutcomes = 13,
    ActionConditions = 14,
    ActionEffects = 15,
    Buttons = 16,
    GuessEffects = 17,
    StatusSets = 20,
    StatusConditions = 21,
    Appearance = 30,
    Species = 31,
    Outfits = 32,
    Evolutions = 33,
    EvolutionConditions = 34,
    SystemRoles = 40,
    Layouts = 41,
    Flow = 42,
    FlowRoles = 43,
};

struct Section
{
    uint16_t type;
    uint16_t count;
    uint16_t recordSize;
    uint32_t offset;
    uint32_t length;
};

struct ActiveAssetScope
{
    uint8_t speciesSlot;
    uint8_t outfitSlot;
};

using ReadAt = bool (*)(void *, uint32_t, uint8_t *, size_t);

struct Source
{
    void *context;
    ReadAt readAt;
    uint32_t size;
};

struct RuntimeTable
{
    Source source;
    Section sections[kMaxSections];
    uint16_t sectionCount;
    uint32_t featureFlags;
    uint32_t schemaFingerprint;
    AssetData::BundleId bundleId;

    const Section *find(SectionType type) const;
};

struct MemorySource
{
    const uint8_t *bytes;
    uint32_t size;
};

bool readMemory(void *context, uint32_t offset, uint8_t *destination, size_t size)
{
    MemorySource &source = *static_cast<MemorySource *>(context);
    if (destination == nullptr || offset > source.size || size > source.size - offset)
        return false;
    memcpy(destination, source.bytes + offset, size);
    return true;
}

struct FileSource
{
    SdBaseFile *file;
};

bool readFile(void *context, uint32_t offset, uint8_t *destination, size_t size)
{
    FileSource &source = *static_cast<FileSource *>(context);
    return source.file != nullptr && destination != nullptr && source.file->seekSet(offset) &&
           readSdBinary(*source.file, destination, size) == static_cast<int>(size);
}

uint16_t readU16(const uint8_t *bytes)
{
    return static_cast<uint16_t>(bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8));
}

int16_t readI16(const uint8_t *bytes)
{
    return static_cast<int16_t>(readU16(bytes));
}

uint32_t readU32(const uint8_t *bytes)
{
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
}

int32_t readI32(const uint8_t *bytes)
{
    return static_cast<int32_t>(readU32(bytes));
}

bool addU32(uint32_t left, uint32_t right, uint32_t &sum)
{
    if (left > UINT32_MAX - right)
        return false;
    sum = left + right;
    return true;
}

uint16_t recordSizeFor(uint16_t type)
{
    switch (type)
    {
    case Profile: return 40;
    case Persistence: return 16;
    case AssetRefs: return 12;
    case Animations: return 8;
    case PetStats: return 12;
    case IdleTriggers: return 12;
    case Actions: return 16;
    case ActionOutcomes: return 12;
    case ActionConditions: return 16;
    case ActionEffects: return 8;
    case Buttons: return 8;
    case GuessEffects: return 8;
    case StatusSets: return 12;
    case StatusConditions: return 12;
    case Appearance: return 8;
    case Species: return 8;
    case Outfits: return 8;
    case Evolutions: return 16;
    case EvolutionConditions: return 12;
    case SystemRoles: return 8;
    case Layouts: return 12;
    case Flow: return 16;
    case FlowRoles: return 8;
    default: return 0;
    }
}

const Section *findSection(const Section *sections, uint16_t count, uint16_t type)
{
    for (uint16_t index = 0; index < count; ++index)
        if (sections[index].type == type)
            return &sections[index];
    return nullptr;
}

const Section *RuntimeTable::find(SectionType type) const
{
    return findSection(sections, sectionCount, type);
}

bool readRecord(const Source &source, const Section &section,
                uint16_t index, uint8_t *record)
{
    if (index >= section.count)
        return false;
    const uint32_t offset = section.offset +
                            static_cast<uint32_t>(index) * section.recordSize;
    return source.readAt(source.context, offset, record, section.recordSize);
}

uint32_t updateCrc32(uint32_t crc, uint8_t byte)
{
    crc ^= byte;
    for (uint8_t bit = 0; bit < 8; ++bit)
        crc = (crc >> 1) ^ ((crc & 1U) ? 0xedb88320UL : 0UL);
    return crc;
}

bool validCrc(const Source &source, uint32_t expected)
{
    uint8_t buffer[64] = {};
    uint32_t crc = 0xffffffffUL;
    uint32_t offset = 0;
    while (offset < source.size)
    {
        const size_t count = source.size - offset < sizeof(buffer)
                                 ? static_cast<size_t>(source.size - offset)
                                 : sizeof(buffer);
        if (!source.readAt(source.context, offset, buffer, count))
            return false;
        for (size_t index = 0; index < count; ++index)
        {
            const uint32_t absolute = offset + static_cast<uint32_t>(index);
            crc = updateCrc32(crc, absolute >= 48 && absolute < 52 ? 0 : buffer[index]);
        }
        offset += static_cast<uint32_t>(count);
    }
    return (crc ^ 0xffffffffUL) == expected;
}

bool readEnvelope(const Source &source, Section *sections, uint16_t &sectionCount,
                  uint32_t &featureFlags, uint32_t &schemaFingerprint,
                  AssetData::BundleId &bundleId)
{
    uint8_t header[kHeaderSize] = {};
    if (source.size < kHeaderSize || source.size > kMaxFileSize ||
        !source.readAt(source.context, 0, header, sizeof(header)) ||
        memcmp(header, kMagic, sizeof(kMagic)) != 0 ||
        readU16(header + 4) != kVersion || readU16(header + 6) != kHeaderSize ||
        readU32(header + 8) != 0x01020304UL || readU32(header + 16) != source.size ||
        readU32(header + 20) != kHeaderSize ||
        readU16(header + 26) != kSectionEntrySize)
        return false;
    for (uint8_t index = 52; index < 64; ++index)
        if (header[index] != 0)
            return false;

    featureFlags = readU32(header + 12);
    if ((featureFlags & ~kKnownFeatures) != 0 ||
        (featureFlags & kPetBehaviorFeature) == 0 ||
        ((featureFlags & kSequentialStatusFeature) != 0 &&
         (featureFlags & kStatusFeature) == 0) ||
        ((featureFlags & kStatusFeature) != 0 &&
         ((featureFlags & kSequentialStatusFeature) != 0) !=
             (ENABLE_SEQUENTIAL_STATUS_SET_SELECTION != 0)))
        return false;

    bool nonzeroBundle = false;
    for (uint8_t index = 0; index < sizeof(bundleId.bytes); ++index)
    {
        bundleId.bytes[index] = header[28 + index];
        nonzeroBundle = nonzeroBundle || bundleId.bytes[index] != 0;
    }
    if (!nonzeroBundle)
        return false;
    schemaFingerprint = readU32(header + 44);
    const uint32_t expectedCrc = readU32(header + 48);

    sectionCount = readU16(header + 24);
    if (sectionCount == 0 || sectionCount > kMaxSections)
        return false;
    const uint32_t directoryBytes = static_cast<uint32_t>(sectionCount) * kSectionEntrySize;
    uint32_t payloadCursor = 0;
    if (!addU32(kHeaderSize, directoryBytes, payloadCursor) || payloadCursor > source.size)
        return false;
    payloadCursor = (payloadCursor + 3U) & ~3UL;

    uint16_t previousType = 0;
    for (uint16_t index = 0; index < sectionCount; ++index)
    {
        uint8_t entry[kSectionEntrySize] = {};
        if (!source.readAt(source.context,
                           kHeaderSize + static_cast<uint32_t>(index) * kSectionEntrySize,
                           entry, sizeof(entry)))
            return false;
        Section &section = sections[index];
        section.type = readU16(entry);
        const uint16_t flags = readU16(entry + 2);
        section.offset = readU32(entry + 4);
        section.count = readU16(entry + 8);
        section.recordSize = readU16(entry + 10);
        section.length = readU32(entry + 12);
        const uint16_t expectedRecordSize = recordSizeFor(section.type);
        const uint32_t expectedLength =
            static_cast<uint32_t>(section.count) * section.recordSize;
        uint32_t end = 0;
        if (section.type <= previousType || flags != 0 || expectedRecordSize == 0 ||
            section.recordSize != expectedRecordSize || section.length != expectedLength ||
            (section.offset & 3U) != 0 || section.offset != payloadCursor ||
            !addU32(section.offset, section.length, end) || end > source.size)
            return false;
        previousType = section.type;
        payloadCursor = (end + 3U) & ~3UL;
        if (payloadCursor < end || payloadCursor > source.size)
            return false;
        for (uint32_t padding = end; padding < payloadCursor; ++padding)
        {
            uint8_t byte = 0;
            if (!source.readAt(source.context, padding, &byte, 1) || byte != 0)
                return false;
        }
    }
    if (payloadCursor != source.size || !validCrc(source, expectedCrc))
        return false;
    return true;
}

bool validCapacity(uint16_t published, uint16_t hard, uint16_t actual)
{
    return published <= hard && actual <= published;
}

bool validateProfileAndPersistence(const Source &source,
                                   const Section *sections,
                                   uint16_t sectionCount,
                                   uint32_t schemaFingerprint)
{
    const Section *profile = findSection(sections, sectionCount, Profile);
    const Section *persistence = findSection(sections, sectionCount, Persistence);
    const Section *assets = findSection(sections, sectionCount, AssetRefs);
    const Section *animations = findSection(sections, sectionCount, Animations);
    const Section *stats = findSection(sections, sectionCount, PetStats);
    const Section *actions = findSection(sections, sectionCount, Actions);
    const Section *outcomes = findSection(sections, sectionCount, ActionOutcomes);
    const Section *conditions = findSection(sections, sectionCount, ActionConditions);
    const Section *effects = findSection(sections, sectionCount, ActionEffects);
    const Section *buttons = findSection(sections, sectionCount, Buttons);
    if (profile == nullptr || persistence == nullptr || assets == nullptr || animations == nullptr ||
        stats == nullptr || actions == nullptr || outcomes == nullptr || buttons == nullptr ||
        profile->count != 1 || persistence->count != 1 || animations->count > 65534 ||
        (findSection(sections, sectionCount, FlowRoles) != nullptr &&
         findSection(sections, sectionCount, FlowRoles)->count > 65534))
        return false;

    uint8_t record[40] = {};
    if (!readRecord(source, *profile, 0, record) || readU32(record + 36) != 0)
        return false;
    const uint16_t published[18] = {
        readU16(record), readU16(record + 2), readU16(record + 4), readU16(record + 6),
        readU16(record + 8), readU16(record + 10), readU16(record + 12), readU16(record + 14),
        readU16(record + 16), readU16(record + 18), readU16(record + 20), readU16(record + 22),
        readU16(record + 24), readU16(record + 26), readU16(record + 28), readU16(record + 30),
        readU16(record + 32), readU16(record + 34)};
    const uint16_t hard[18] = {10, 16, 8, 32, 240, 24, 8, 40, 5, 15,
                               255, 255, 255, 1020, 65534, 65534, 65534, 65534};
    const uint16_t actual[18] = {
        stats->count,
        findSection(sections, sectionCount, IdleTriggers) == nullptr ? 0 : findSection(sections, sectionCount, IdleTriggers)->count,
        actions->count,
        conditions == nullptr ? 0 : conditions->count,
        effects == nullptr ? 0 : effects->count,
        outcomes->count,
        buttons->count,
        findSection(sections, sectionCount, GuessEffects) == nullptr ? 0 : findSection(sections, sectionCount, GuessEffects)->count,
        findSection(sections, sectionCount, StatusSets) == nullptr ? 0 : findSection(sections, sectionCount, StatusSets)->count,
        findSection(sections, sectionCount, StatusConditions) == nullptr ? 0 : findSection(sections, sectionCount, StatusConditions)->count,
        findSection(sections, sectionCount, Species) == nullptr ? 0 : findSection(sections, sectionCount, Species)->count,
        findSection(sections, sectionCount, Outfits) == nullptr ? 0 : findSection(sections, sectionCount, Outfits)->count,
        findSection(sections, sectionCount, Evolutions) == nullptr ? 0 : findSection(sections, sectionCount, Evolutions)->count,
        findSection(sections, sectionCount, EvolutionConditions) == nullptr ? 0 : findSection(sections, sectionCount, EvolutionConditions)->count,
        assets->count,
        findSection(sections, sectionCount, SystemRoles) == nullptr ? 0 : findSection(sections, sectionCount, SystemRoles)->count,
        findSection(sections, sectionCount, Layouts) == nullptr ? 0 : findSection(sections, sectionCount, Layouts)->count,
        findSection(sections, sectionCount, Flow) == nullptr ? 0 : findSection(sections, sectionCount, Flow)->count};
    for (uint8_t index = 0; index < 18; ++index)
        if (!validCapacity(published[index], hard[index], actual[index]))
            return false;
    if (published[0] > kPetBehaviorSlotCount || published[2] > kMaxPetBehaviorActions ||
        published[3] > kMaxPetBehaviorActionConditions || published[6] > kPetBehaviorButtonCount ||
        published[8] > kMaxStatusSets || published[9] > kMaxStatusSets * kMaxStatusConditions)
        return false;

    uint8_t persistenceRecord[16] = {};
    return readRecord(source, *persistence, 0, persistenceRecord) &&
           readU32(persistenceRecord + 4) == schemaFingerprint &&
           readU32(persistenceRecord + 12) == 0;
}

bool validateAssetCatalog(const Source &source,
                          const Section &assets,
                          const Section &animations)
{
    uint16_t expectedFirstAsset = 0;
    uint16_t previousRuntimeId = 0;
    for (uint16_t animationIndex = 0; animationIndex < animations.count; ++animationIndex)
    {
        uint8_t animation[8] = {};
        if (!readRecord(source, animations, animationIndex, animation))
            return false;
        const uint16_t runtimeId = readU16(animation + 2);
        const uint16_t firstAsset = readU16(animation + 4);
        const uint16_t assetCount = readU16(animation + 6);
        if (readU16(animation) != animationIndex || runtimeId == 0 ||
            runtimeId > AssetData::kMaxRuntimeAnimationId || runtimeId <= previousRuntimeId ||
            firstAsset != expectedFirstAsset || assetCount == 0 ||
            static_cast<uint32_t>(firstAsset) + assetCount > assets.count)
            return false;
        previousRuntimeId = runtimeId;
        expectedFirstAsset = static_cast<uint16_t>(firstAsset + assetCount);
        bool shared = false;
        bool hasPreviousScope = false;
        uint8_t previousKind = 0;
        uint8_t previousSpecies = 0;
        uint8_t previousOutfit = 0;
        for (uint16_t offset = 0; offset < assetCount; ++offset)
        {
            uint8_t asset[12] = {};
            const uint16_t assetIndex = static_cast<uint16_t>(firstAsset + offset);
            if (!readRecord(source, assets, assetIndex, asset) ||
                readU16(asset) != assetIndex || readU16(asset + 6) != runtimeId ||
                readU16(asset + 8) != 0 || readU16(asset + 10) != 0 || asset[5] != 0)
                return false;
            const bool canonicalAfterPrevious = !hasPreviousScope || asset[2] > previousKind ||
                (asset[2] == previousKind &&
                 (asset[3] > previousSpecies ||
                  (asset[3] == previousSpecies && asset[4] > previousOutfit)));
            if (!canonicalAfterPrevious)
                return false;
            hasPreviousScope = true;
            previousKind = asset[2];
            previousSpecies = asset[3];
            previousOutfit = asset[4];
            if (asset[2] == 2)
            {
                if (asset[3] != 0 || asset[4] != 0 || assetCount != 1)
                    return false;
                shared = true;
            }
            else if (asset[2] != 1 || asset[3] == 0 || asset[4] == 0 || shared)
                return false;
        }
    }
    return expectedFirstAsset == assets.count;
}

bool readRuntimeTable(const Source &source,
                      const AssetData::RuntimeManifest *expectedManifest,
                      RuntimeTable &table)
{
    table = {};
    table.source = source;
    if (!readEnvelope(source, table.sections, table.sectionCount,
                      table.featureFlags, table.schemaFingerprint, table.bundleId) ||
        (expectedManifest != nullptr &&
         !AssetData::sameBundleId(table.bundleId, expectedManifest->bundleId)) ||
        !validateProfileAndPersistence(source, table.sections, table.sectionCount,
                                       table.schemaFingerprint))
        return false;
    const Section *assets = table.find(AssetRefs);
    const Section *animations = table.find(Animations);
    return assets != nullptr && animations != nullptr &&
           validateAssetCatalog(source, *assets, *animations);
}

bool resolveAnimation(const Source &source,
                      const Section &assets,
                      const Section &animations,
                      uint16_t animationRef,
                      const ActiveAssetScope &scope,
                      AssetData::AnimationRef &resolved)
{
    resolved = {};
    if (animationRef == kNone16 || animationRef >= animations.count)
        return false;
    uint8_t animation[8] = {};
    if (!readRecord(source, animations, animationRef, animation) ||
        readU16(animation) != animationRef)
        return false;
    const uint16_t runtimeId = readU16(animation + 2);
    const uint16_t firstAsset = readU16(animation + 4);
    const uint16_t assetCount = readU16(animation + 6);
    for (uint16_t offset = 0; offset < assetCount; ++offset)
    {
        uint8_t asset[12] = {};
        if (!readRecord(source, assets, static_cast<uint16_t>(firstAsset + offset), asset))
            return false;
        if (asset[2] == 2 ||
            (asset[2] == 1 && asset[3] == scope.speciesSlot && asset[4] == scope.outfitSlot))
        {
            resolved.speciesSlot = asset[2] == 2 ? 0 : asset[3];
            resolved.outfitSlot = asset[2] == 2 ? 0 : asset[4];
            resolved.animationId = runtimeId;
            return resolved.valid();
        }
    }
    return false;
}

bool validOptionalAnimationRef(const Source &source, const Section &animations,
                               uint16_t animationRef)
{
    if (animationRef == kNone16)
        return true;
    uint8_t animation[8] = {};
    return animationRef < animations.count &&
           readRecord(source, animations, animationRef, animation) &&
           readU16(animation) == animationRef;
}

void clearOwnedBehavior(PetBehaviorConfig &config)
{
    memset(config.stats, 0, sizeof(config.stats));
    memset(config.actions, 0, sizeof(config.actions));
    memset(config.randomOutcomes, 0, sizeof(config.randomOutcomes));
    memset(config.actionConditions, 0, sizeof(config.actionConditions));
    memset(config.actionEffects, 0, sizeof(config.actionEffects));
    memset(config.randomOutcomeEffects, 0, sizeof(config.randomOutcomeEffects));
    memset(config.buttons, 0, sizeof(config.buttons));
    memset(&config.statusSets, 0, sizeof(config.statusSets));
    config.statCount = 0;
    config.actionCount = 0;
    config.actionConditionCount = 0;
    config.actionEffectCount = 0;
    config.randomOutcomeEffectCount = 0;
    config.buttonCount = 0;
}

bool decodeStats(const Source &source, const Section &section, PetBehaviorConfig &config)
{
    if (section.count == 0 || section.count > kPetBehaviorSlotCount)
        return false;
    for (uint16_t index = 0; index < section.count; ++index)
    {
        uint8_t record[12] = {};
        if (!readRecord(source, section, index, record) || record[0] != index ||
            record[1] != 0 || readU16(record + 10) != 0)
            return false;
        PetBehaviorStatConfig &stat = config.stats[index];
        stat.active = true;
        stat.initialValue = readI16(record + 2);
        stat.minValue = readI16(record + 4);
        stat.maxValue = readI16(record + 6);
        stat.dailyChange = readI16(record + 8);
        if (stat.minValue > stat.initialValue || stat.initialValue > stat.maxValue)
            return false;
    }
    config.statCount = static_cast<uint8_t>(section.count);
    return true;
}

bool decodeActions(const Source &source,
                   const Section &actions,
                   const Section &outcomes,
                   const Section *conditions,
                   const Section *effects,
                   const Section &assets,
                   const Section &animations,
                   const ActiveAssetScope &scope,
                   PetBehaviorConfig &config)
{
    if (actions.count > kMaxPetBehaviorActions || outcomes.count > 24 ||
        (conditions != nullptr && conditions->count > kMaxPetBehaviorActionConditions) ||
        (effects != nullptr && effects->count > 240))
        return false;
    uint16_t nextOutcome = 0;
    uint16_t nextCondition = 0;
    uint16_t nextEffect = 0;
    for (uint16_t actionIndex = 0; actionIndex < actions.count; ++actionIndex)
    {
        uint8_t actionRecord[16] = {};
        if (!readRecord(source, actions, actionIndex, actionRecord) ||
            actionRecord[0] != actionIndex || actionRecord[7] != 0 || actionRecord[11] != 0 ||
            readU32(actionRecord + 12) != 0 || readU16(actionRecord + 4) != nextOutcome ||
            readU16(actionRecord + 8) != nextCondition || actionRecord[1] > 2 ||
            (actionRecord[3] & ~1U) != 0)
            return false;
        const uint8_t mode = actionRecord[1];
        const uint8_t outcomeCount = actionRecord[6];
        const uint8_t conditionCount = actionRecord[10];
        const bool hasFallback = (actionRecord[3] & 1U) != 0;
        if ((mode < 2 && outcomeCount != 1) ||
            (mode == 2 && (outcomeCount < 2 || outcomeCount > 3)) ||
            (mode == 1 && (conditionCount == 0 || conditionCount > 4)) ||
            (mode != 1 && conditionCount != 0) ||
            (mode != 1 && hasFallback) ||
            static_cast<uint32_t>(nextOutcome) + outcomeCount > outcomes.count ||
            (conditionCount != 0 && conditions == nullptr) ||
            static_cast<uint32_t>(nextCondition) + conditionCount >
                (conditions == nullptr ? 0 : conditions->count))
            return false;

        PetBehaviorActionConfig &action = config.actions[actionIndex];
        action.active = true;
        action.mode = static_cast<PetBehaviorActionMode>(mode);
        action.suspendDailyChangeDays = actionRecord[2];
        action.hasFallbackAnimation = hasFallback;

        for (uint8_t outcomeSlot = 0; outcomeSlot < outcomeCount; ++outcomeSlot)
        {
            uint8_t outcomeRecord[12] = {};
            if (!readRecord(source, outcomes, nextOutcome, outcomeRecord) ||
                outcomeRecord[0] != actionIndex || outcomeRecord[1] != outcomeSlot ||
                readU16(outcomeRecord + 6) != nextEffect || readU16(outcomeRecord + 10) != 0 ||
                static_cast<uint32_t>(nextEffect) + readU16(outcomeRecord + 8) >
                    (effects == nullptr ? 0 : effects->count))
                return false;
            const uint8_t weight = outcomeRecord[2];
            const uint8_t playbackCount = outcomeRecord[3];
            const uint16_t animationRef = readU16(outcomeRecord + 4);
            const uint16_t effectCount = readU16(outcomeRecord + 8);
            if (effectCount > kPetBehaviorSlotCount ||
                (mode == 2 ? (weight == 0 || weight > 100) : weight != 0))
                return false;
            AssetData::AnimationRef animation = {};
            const bool missingConditionalFallback = mode == 1 && !hasFallback;
            if (missingConditionalFallback)
            {
                if (animationRef != kNone16 || playbackCount != 0)
                    return false;
            }
            else if (playbackCount == 0 || playbackCount > 5 ||
                     !resolveAnimation(source, assets, animations, animationRef, scope, animation))
                return false;

            if (mode == 2)
            {
                PetBehaviorRandomOutcomeConfig &outcome =
                    config.randomOutcomes[actionIndex][outcomeSlot];
                outcome.active = true;
                outcome.weight = weight;
                outcome.animationPlayback.animation = animation;
                outcome.animationPlayback.playbackCount = playbackCount;
            }
            else
            {
                action.animationPlayback.animation = animation;
                action.animationPlayback.playbackCount = playbackCount;
                if (mode == 0)
                    action.hasFallbackAnimation = true;
            }

            bool affected[kPetBehaviorSlotCount] = {};
            for (uint16_t effectOffset = 0; effectOffset < effectCount; ++effectOffset)
            {
                uint8_t effectRecord[8] = {};
                if (effects == nullptr || !readRecord(source, *effects, nextEffect, effectRecord) ||
                    effectRecord[0] != actionIndex || effectRecord[1] != outcomeSlot ||
                    effectRecord[2] >= config.statCount || effectRecord[3] > 1 ||
                    readU16(effectRecord + 6) != 0 || affected[effectRecord[2]])
                    return false;
                affected[effectRecord[2]] = true;
                if (mode == 2)
                {
                    if (config.randomOutcomeEffectCount >= kMaxPetBehaviorRandomOutcomeEffects)
                        return false;
                    PetBehaviorRandomOutcomeEffectConfig &effect =
                        config.randomOutcomeEffects[config.randomOutcomeEffectCount++];
                    effect.active = true;
                    effect.actionSlot = static_cast<uint8_t>(actionIndex);
                    effect.outcomeSlot = outcomeSlot;
                    effect.statSlot = effectRecord[2];
                    effect.operation = static_cast<PetBehaviorEffectOperation>(effectRecord[3]);
                    effect.value = readI16(effectRecord + 4);
                }
                else
                {
                    if (config.actionEffectCount >= kMaxPetBehaviorActionEffects)
                        return false;
                    PetBehaviorActionEffectConfig &effect =
                        config.actionEffects[config.actionEffectCount++];
                    effect.active = true;
                    effect.actionSlot = static_cast<uint8_t>(actionIndex);
                    effect.statSlot = effectRecord[2];
                    effect.operation = static_cast<PetBehaviorEffectOperation>(effectRecord[3]);
                    effect.value = readI16(effectRecord + 4);
                }
                ++nextEffect;
            }
            ++nextOutcome;
        }

        uint8_t previousPriority = 0;
        for (uint8_t conditionSlot = 0; conditionSlot < conditionCount; ++conditionSlot)
        {
            uint8_t conditionRecord[16] = {};
            if (conditions == nullptr || !readRecord(source, *conditions, nextCondition, conditionRecord) ||
                conditionRecord[0] != actionIndex || conditionRecord[2] > 1 ||
                conditionRecord[3] > 4 || conditionRecord[5] == 0 || conditionRecord[5] > 5 ||
                readU32(conditionRecord + 12) != 0 ||
                (conditionSlot != 0 && conditionRecord[1] <= previousPriority) ||
                (conditionRecord[2] == 0 && conditionRecord[4] >= config.statCount) ||
                (conditionRecord[2] == 1 && conditionRecord[4] != 0))
                return false;
            previousPriority = conditionRecord[1];
            PetBehaviorActionConditionConfig &condition =
                config.actionConditions[config.actionConditionCount++];
            condition.active = true;
            condition.actionSlot = static_cast<uint8_t>(actionIndex);
            condition.priority = conditionRecord[1];
            condition.source = static_cast<PetBehaviorActionConditionSource>(conditionRecord[2]);
            condition.comparison = static_cast<PetBehaviorActionConditionOperator>(conditionRecord[3]);
            condition.statSlot = conditionRecord[4];
            condition.animationPlayback.playbackCount = conditionRecord[5];
            condition.threshold = readI32(conditionRecord + 8);
            if (!resolveAnimation(source, assets, animations, readU16(conditionRecord + 6), scope,
                                  condition.animationPlayback.animation))
                return false;
            ++nextCondition;
        }
    }
    config.actionCount = static_cast<uint8_t>(actions.count);
    return nextOutcome == outcomes.count &&
           nextCondition == (conditions == nullptr ? 0 : conditions->count) &&
           nextEffect == (effects == nullptr ? 0 : effects->count);
}

bool decodeButtons(const Source &source, const Section &buttons, PetBehaviorConfig &config)
{
    if (buttons.count != kPetBehaviorButtonCount)
        return false;
    for (uint16_t index = 0; index < buttons.count; ++index)
    {
        uint8_t record[8] = {};
        if (!readRecord(source, buttons, index, record) || record[0] != index + 1 ||
            record[1] > 2 || readU32(record + 4) != 0)
            return false;
        PetBehaviorButtonConfig &button = config.buttons[index];
        button.active = true;
        button.kind = static_cast<PetBehaviorButtonKind>(record[1]);
        const uint16_t target = readU16(record + 2);
        if (button.kind == PetBehaviorButtonKind::Empty)
        {
            if (target != kNone16)
                return false;
        }
        else if (button.kind == PetBehaviorButtonKind::UserAction)
        {
            if (target >= config.actionCount)
                return false;
            button.actionSlot = static_cast<uint8_t>(target);
        }
        else
        {
            if (findCompiledSystemCommand(static_cast<RuntimeSystemCommandId>(target)) == nullptr)
                return false;
            button.systemCommandId = static_cast<RuntimeSystemCommandId>(target);
        }
    }
    config.buttonCount = static_cast<uint8_t>(buttons.count);
    return true;
}

bool decodeStatus(const Source &source,
                  uint32_t featureFlags,
                  const Section *sets,
                  const Section *conditions,
                  const Section &assets,
                  const Section &animations,
                  const ActiveAssetScope &scope,
                  PetBehaviorConfig &config)
{
    const bool enabled = (featureFlags & kStatusFeature) != 0;
    if (!enabled)
        return sets == nullptr && conditions == nullptr;
    if (sets == nullptr || conditions == nullptr || sets->count == 0 ||
        sets->count > kMaxStatusSets || conditions->count > kMaxStatusSets * kMaxStatusConditions)
        return false;
    uint16_t nextCondition = 0;
    for (uint16_t setIndex = 0; setIndex < sets->count; ++setIndex)
    {
        uint8_t setRecord[12] = {};
        if (!readRecord(source, *sets, setIndex, setRecord) || setRecord[0] != setIndex ||
            setRecord[1] > kMaxStatusConditions || readU16(setRecord + 4) != nextCondition ||
            (setRecord[8] & ~1U) != 0 || setRecord[9] != 0 || readU16(setRecord + 10) != 0 ||
            static_cast<uint32_t>(nextCondition) + setRecord[1] > conditions->count)
            return false;
        StatusSetConfig &set = config.statusSets.sets[setIndex];
        set.conditionCount = setRecord[1];
        if (!resolveAnimation(source, assets, animations, readU16(setRecord + 2),
                              scope, set.animation))
            return false;
        const bool playOnce = (setRecord[8] & 1U) != 0;
        uint16_t requiredFrames = 1;
        for (uint8_t conditionIndex = 0; conditionIndex < set.conditionCount; ++conditionIndex)
        {
            uint8_t conditionRecord[12] = {};
            if (!readRecord(source, *conditions, nextCondition, conditionRecord) ||
                conditionRecord[0] != setIndex || conditionRecord[1] > 1 ||
                conditionRecord[3] == 0 || conditionRecord[3] > 32 ||
                (conditionRecord[1] == 0 && conditionRecord[2] >= config.statCount) ||
                (conditionRecord[1] == 1 && conditionRecord[2] != 0) ||
                readI32(conditionRecord + 4) > readI32(conditionRecord + 8))
                return false;
            StatusSetCondition &condition = set.conditions[conditionIndex];
            condition.source = static_cast<StatusConditionSource>(conditionRecord[1]);
            condition.statSlot = conditionRecord[2];
            condition.levels = conditionRecord[3];
            condition.minValue = readI32(conditionRecord + 4);
            condition.maxValue = readI32(conditionRecord + 8);
            requiredFrames = static_cast<uint16_t>(requiredFrames * condition.levels);
            if (requiredFrames > 256)
                return false;
            ++nextCondition;
        }
        if ((set.conditionCount == 0) != playOnce ||
            readU16(setRecord + 6) != requiredFrames)
            return false;
    }
    config.statusSets.count = static_cast<uint8_t>(sets->count);
    return nextCondition == conditions->count;
}

bool validComposedRecords(const PetBehaviorConfig &config)
{
    uint8_t idleTriggerCount = 0;
    for (uint8_t index = 0; index < kMaxPetBehaviorIdleTriggers; ++index)
    {
        const PetBehaviorIdleTriggerConfig &trigger = config.idleTriggers[index];
        if (!trigger.active)
            continue;
        if (trigger.statSlot >= config.statCount || !config.stats[trigger.statSlot].active ||
            !trigger.animation.valid())
            return false;
        ++idleTriggerCount;
    }
    if (idleTriggerCount != config.idleTriggerCount)
        return false;
#if ENABLE_GUESS_GAME
    bool affected[kPetBehaviorGuessOutcomeCount][kPetBehaviorSlotCount] = {};
    for (uint8_t index = 0; index < config.guessEffectCount; ++index)
    {
        const PetBehaviorGuessEffectConfig &effect = config.guessEffects[index];
        const uint8_t outcome = static_cast<uint8_t>(effect.outcome);
        if (!effect.active || outcome >= kPetBehaviorGuessOutcomeCount ||
            effect.statSlot >= config.statCount || !config.stats[effect.statSlot].active ||
            affected[outcome][effect.statSlot])
            return false;
        affected[outcome][effect.statSlot] = true;
    }
#endif
    return true;
}

bool decodeRuntimeTableBehavior(const RuntimeTable &table,
                                const AssetData::RuntimeManifest &manifest,
                                uint8_t speciesSlot,
                                uint8_t outfitSlot,
                                PetBehaviorConfig &config)
{
    if (speciesSlot == 0 || outfitSlot == 0)
        return false;
    const Source &source = table.source;
    const Section *assets = table.find(AssetRefs);
    const Section *animations = table.find(Animations);
    const Section *stats = table.find(PetStats);
    const Section *actions = table.find(Actions);
    const Section *outcomes = table.find(ActionOutcomes);
    const Section *actionConditions = table.find(ActionConditions);
    const Section *effects = table.find(ActionEffects);
    const Section *buttons = table.find(Buttons);
    if (assets == nullptr || animations == nullptr || stats == nullptr || actions == nullptr ||
        outcomes == nullptr || buttons == nullptr)
        return false;

    PetBehaviorConfig candidate = config;
    clearOwnedBehavior(candidate);
    candidate.assetManifest = manifest;
    candidate.activeSpeciesSlot = speciesSlot;
    candidate.activeOutfitSlot = outfitSlot;
    candidate.schemaFingerprint = table.schemaFingerprint;
    const ActiveAssetScope scope = {speciesSlot, outfitSlot};
    if (!decodeStats(source, *stats, candidate) ||
        !decodeActions(source, *actions, *outcomes, actionConditions, effects,
                       *assets, *animations, scope, candidate) ||
        !decodeButtons(source, *buttons, candidate) ||
        !decodeStatus(source, table.featureFlags,
                      table.find(StatusSets), table.find(StatusConditions),
                      *assets, *animations, scope, candidate) ||
        !validComposedRecords(candidate))
        return false;
    config = candidate;
    return true;
}

enum class AppearanceQueryKind : uint8_t { Validate, Initial, Evolution, Species, Outfits, Preview };

struct AppearanceQuery
{
    AppearanceQueryKind kind = AppearanceQueryKind::Validate;
    const ActivePetBehaviorStatSlots *activeSlots = nullptr;
    const PetStatSnapshot *stats = nullptr;
    uint8_t speciesSlot = 0;
    uint8_t outfitSlot = 0;
    uint8_t *slots = nullptr;
    size_t capacity = 0;
    size_t count = 0;
    AppearanceSelection *selection = nullptr;
    OutfitPreview *preview = nullptr;
    AssetData::AnimationRef *idleAnimation = nullptr;
};

bool hasOutfit(const Source &source, const Section &outfits,
               uint8_t speciesSlot, uint8_t outfitSlot)
{
    for (uint16_t index = 0; index < outfits.count; ++index)
    {
        uint8_t record[8] = {};
        if (!readRecord(source, outfits, index, record))
            return false;
        if (record[0] == speciesSlot && record[1] == outfitSlot)
            return true;
    }
    return false;
}

bool conditionMatches(const uint8_t *record, const PetStatSnapshot &stats,
                      const ActivePetBehaviorStatSlots &activeSlots)
{
    const uint8_t sourceKind = record[2];
    const uint8_t statSlot = record[3];
    const int32_t minimum = readI32(record + 4);
    const int32_t maximum = readI32(record + 8);
    if (sourceKind > 3 || minimum > maximum ||
        ((sourceKind == 1 || sourceKind >= 2) && statSlot != 0) ||
        (sourceKind == 0 && !activeSlots.contains(statSlot)))
        return false;
    int32_t value = 0;
    switch (sourceKind)
    {
    case 0: value = stats.customStats[statSlot]; break;
    case 1: value = static_cast<int32_t>(stats.stage_days); break;
    case 2: value = stats.speciesSlot; break;
    case 3: value = stats.outfitSlot; break;
    default: return false;
    }
    return value >= minimum && value <= maximum;
}

bool decodeRuntimeTableAppearance(const RuntimeTable &table,
                                  BundleReader &bundleReader,
                                  AppearanceQuery &query)
{
    const Source &source = table.source;
    const uint32_t featureFlags = table.featureFlags;
    const Section *assets = table.find(AssetRefs);
    const Section *animations = table.find(Animations);
    const Section *appearance = table.find(Appearance);
    const Section *species = table.find(Species);
    const Section *outfits = table.find(Outfits);
    const Section *evolutions = table.find(Evolutions);
    const Section *conditions = table.find(EvolutionConditions);
    const bool evolutionEnabled = (featureFlags & (1UL << 3)) != 0;
    if ((featureFlags & (1UL << 2)) == 0 || assets == nullptr || animations == nullptr ||
        appearance == nullptr || species == nullptr || outfits == nullptr ||
        appearance->count != 1 || species->count == 0 ||
        (evolutionEnabled && (evolutions == nullptr || conditions == nullptr)) ||
        (!evolutionEnabled && (evolutions != nullptr || conditions != nullptr)))
        return false;

    uint8_t appearanceRecord[8] = {};
    if (!readRecord(source, *appearance, 0, appearanceRecord) || readU32(appearanceRecord + 4) != 0 ||
        !hasOutfit(source, *outfits, appearanceRecord[0], appearanceRecord[1]))
        return false;
    AssetData::AnimationRef initialIdle = {};
    const ActiveAssetScope initialScope = {appearanceRecord[0], appearanceRecord[1]};
    if (!resolveAnimation(source, *assets, *animations, readU16(appearanceRecord + 2), initialScope, initialIdle) ||
        !AssetData::animationReferenceExists(bundleReader, initialIdle))
        return false;

    uint16_t expectedOutfit = 0;
    for (uint16_t speciesIndex = 0; speciesIndex < species->count; ++speciesIndex)
    {
        uint8_t record[8] = {};
        if (!readRecord(source, *species, speciesIndex, record) || record[0] != speciesIndex + 1 ||
            record[1] == 0 || readU16(record + 2) != expectedOutfit || readU16(record + 4) == 0 ||
            readU16(record + 6) != 0 ||
            static_cast<uint32_t>(expectedOutfit) + readU16(record + 4) > outfits->count)
            return false;
        const uint16_t outfitCount = readU16(record + 4);
        bool entryFound = false;
        for (uint16_t offset = 0; offset < outfitCount; ++offset)
        {
            uint8_t outfit[8] = {};
            const uint16_t outfitIndex = static_cast<uint16_t>(expectedOutfit + offset);
            if (!readRecord(source, *outfits, outfitIndex, outfit) || outfit[0] != record[0] ||
                outfit[1] != offset + 1 || readU32(outfit + 4) != 0)
                return false;
            AssetData::AnimationRef preview = {};
            const ActiveAssetScope scope = {outfit[0], outfit[1]};
            if (!resolveAnimation(source, *assets, *animations, readU16(outfit + 2), scope, preview) ||
                !AssetData::animationReferenceExists(bundleReader, preview))
                return false;
            entryFound = entryFound || outfit[1] == record[1];
            if (query.kind == AppearanceQueryKind::Outfits && query.speciesSlot == record[0])
            {
                if (query.count >= query.capacity)
                    return false;
                query.slots[query.count++] = outfit[1];
            }
            if (query.kind == AppearanceQueryKind::Preview && query.speciesSlot == outfit[0] &&
                query.outfitSlot == outfit[1])
            {
                query.preview->speciesSlot = outfit[0];
                query.preview->outfitSlot = outfit[1];
                query.preview->animation = preview;
            }
        }
        if (!entryFound)
            return false;
        if (query.kind == AppearanceQueryKind::Species)
        {
            if (query.count >= query.capacity)
                return false;
            query.slots[query.count++] = record[0];
        }
        expectedOutfit = static_cast<uint16_t>(expectedOutfit + outfitCount);
    }
    if (expectedOutfit != outfits->count)
        return false;

    if (evolutionEnabled)
    {
        uint16_t nextCondition = 0;
        uint16_t previousPriority = 0;
        for (uint16_t index = 0; index < evolutions->count; ++index)
        {
            uint8_t evolution[16] = {};
            if (!readRecord(source, *evolutions, index, evolution) ||
                (index != 0 && readU16(evolution) <= previousPriority) || evolution[2] == 0 ||
                evolution[2] > species->count || !hasOutfit(source, *outfits, evolution[3], evolution[4]) ||
                evolution[5] > 4 || readU16(evolution + 6) != nextCondition ||
                readU16(evolution + 10) != 0 || readU32(evolution + 12) != 0 ||
                !validOptionalAnimationRef(source, *animations, readU16(evolution + 8)) ||
                static_cast<uint32_t>(nextCondition) + evolution[5] > conditions->count)
                return false;
            previousPriority = readU16(evolution);
            bool matched = query.kind == AppearanceQueryKind::Evolution && query.stats != nullptr &&
                           query.stats->speciesSlot == evolution[2];
            for (uint8_t offset = 0; offset < evolution[5]; ++offset)
            {
                uint8_t condition[12] = {};
                if (!readRecord(source, *conditions, static_cast<uint16_t>(nextCondition + offset), condition) ||
                    readU16(condition) != index || condition[2] > 3 ||
                    readI32(condition + 4) > readI32(condition + 8) ||
                    ((condition[2] == 1 || condition[2] >= 2) && condition[3] != 0) ||
                    (condition[2] == 0 &&
                     ((query.activeSlots != nullptr && !query.activeSlots->contains(condition[3])) ||
                      (query.activeSlots == nullptr && condition[3] >= kPetBehaviorSlotCount))))
                    return false;
                if (query.activeSlots != nullptr && query.stats != nullptr)
                    matched = matched && conditionMatches(condition, *query.stats, *query.activeSlots);
            }
            nextCondition = static_cast<uint16_t>(nextCondition + evolution[5]);
            if (matched && query.selection != nullptr && query.selection->speciesSlot == 0)
            {
                const ActiveAssetScope scope = {query.stats->speciesSlot, query.stats->outfitSlot};
                AssetData::AnimationRef animation = {};
                const uint16_t animationRef = readU16(evolution + 8);
                if (animationRef != kNone16 &&
                    (!resolveAnimation(source, *assets, *animations, animationRef, scope, animation) ||
                     !AssetData::animationReferenceExists(bundleReader, animation)))
                    return false;
                query.selection->speciesSlot = evolution[3];
                query.selection->outfitSlot = evolution[4];
                query.selection->evolutionAnimation = animation;
            }
        }
        if (nextCondition != conditions->count)
            return false;
    }
    if (query.kind == AppearanceQueryKind::Initial)
    {
        query.selection->speciesSlot = appearanceRecord[0];
        query.selection->outfitSlot = appearanceRecord[1];
        if (query.idleAnimation != nullptr)
            *query.idleAnimation = initialIdle;
    }
    return true;
}

bool compiledFeaturesAccept(uint32_t flags)
{
#if !ENABLE_GUESS_GAME
    if ((flags & kGuessGameFeature) != 0)
        return false;
#endif
#if !ENABLE_COMMAND_PREDICT
    if ((flags & kPredictFeature) != 0)
        return false;
#endif
#if !ENABLE_STARTUP_ANIMATION
    if ((flags & kStartupAnimationFeature) != 0)
        return false;
#endif
#if !ENABLE_FIRST_START_ANIMATION
    if ((flags & kFirstStartAnimationFeature) != 0)
        return false;
#endif
#if !ENABLE_DYNAMIC_ACTION_LAYOUT
    if ((flags & kDynamicActionLayoutFeature) != 0)
        return false;
#endif
#if !ENABLE_FIRST_LAUNCH_SELECTION
    if ((flags & kFirstLaunchSelectionFeature) != 0)
        return false;
#endif
#if !ENABLE_OUTFIT_CHOOSE_ANIMATION
    if ((flags & kOutfitChooseAnimationFeature) != 0)
        return false;
#endif
    return (flags & kFirstStartAnimationFeature) == 0 ||
           (flags & kStartupAnimationFeature) != 0;
}

bool requireRole(const bool *present, FirmwarePlaybackRole role)
{
    const size_t index = static_cast<size_t>(role);
    return index < kFirmwarePlaybackRoleCount && present[index];
}

FirmwarePlaybackRole flowRoleAt(uint8_t flow, uint8_t slot)
{
    static constexpr FirmwarePlaybackRole kPredict[] = {
        FirmwarePlaybackRole::PredAnim, FirmwarePlaybackRole::Predict1,
        FirmwarePlaybackRole::Predict2, FirmwarePlaybackRole::Predict3,
        FirmwarePlaybackRole::Predict4, FirmwarePlaybackRole::Predict5,
        FirmwarePlaybackRole::Predict6, FirmwarePlaybackRole::Predict7,
        FirmwarePlaybackRole::Predict8, FirmwarePlaybackRole::Predict9,
        FirmwarePlaybackRole::Predict10, FirmwarePlaybackRole::Predict11};
    static constexpr FirmwarePlaybackRole kGuess[] = {
        FirmwarePlaybackRole::GuessWin, FirmwarePlaybackRole::GuessLoss,
        FirmwarePlaybackRole::GuessRight, FirmwarePlaybackRole::GuessWrong,
        FirmwarePlaybackRole::GuessItem1, FirmwarePlaybackRole::GuessItem2,
        FirmwarePlaybackRole::GuessItem3, FirmwarePlaybackRole::GuessLL,
        FirmwarePlaybackRole::GuessLR, FirmwarePlaybackRole::GuessRL,
        FirmwarePlaybackRole::GuessRR, FirmwarePlaybackRole::GuessItem4,
        FirmwarePlaybackRole::GuessStart};
    static constexpr FirmwarePlaybackRole kStartup[] = {
        FirmwarePlaybackRole::Start, FirmwarePlaybackRole::StartIntro,
        FirmwarePlaybackRole::FirstStart};
    const FirmwarePlaybackRole *roles = nullptr;
    size_t count = 0;
    if (flow == 0) { roles = kPredict; count = sizeof(kPredict) / sizeof(kPredict[0]); }
    else if (flow == 1) { roles = kGuess; count = sizeof(kGuess) / sizeof(kGuess[0]); }
    else if (flow == 2) { roles = kStartup; count = sizeof(kStartup) / sizeof(kStartup[0]); }
    return slot < count ? roles[slot] : FirmwarePlaybackRole::None;
}

bool decodeRuntimeTableFlow(const RuntimeTable &table,
                            uint8_t speciesSlot,
                            uint8_t outfitSlot,
                            PetBehaviorConfig &config)
{
    if (speciesSlot == 0 || outfitSlot == 0)
        return false;
    const Source &source = table.source;
    const uint32_t featureFlags = table.featureFlags;
    if (!compiledFeaturesAccept(featureFlags))
        return false;

    const Section *assets = table.find(AssetRefs);
    const Section *animations = table.find(Animations);
    const Section *roles = table.find(SystemRoles);
    const Section *layouts = table.find(Layouts);
    const Section *flows = table.find(Flow);
    const Section *flowRoles = table.find(FlowRoles);
    if (assets == nullptr || animations == nullptr || roles == nullptr ||
        ((featureFlags & kDynamicActionLayoutFeature) != 0 && layouts == nullptr) ||
        ((featureFlags & kDynamicActionLayoutFeature) == 0 && layouts != nullptr) ||
        ((flows == nullptr) != (flowRoles == nullptr)))
        return false;

    PetBehaviorConfig candidate = config;
    memset(candidate.systemAnimations, 0, sizeof(candidate.systemAnimations));
    memset(candidate.actionLayoutVersions, 0, sizeof(candidate.actionLayoutVersions));
    memset(candidate.layouts, 0, sizeof(candidate.layouts));
    candidate.layoutUnselected = {};
    candidate.layoutSelected = {};
    candidate.layoutCount = 0;
    const ActiveAssetScope scope = {speciesSlot, outfitSlot};
    bool rolePresent[kFirmwarePlaybackRoleCount] = {};
    uint8_t previousRole = 0;
    for (uint16_t index = 0; index < roles->count; ++index)
    {
        uint8_t record[8] = {};
        AssetData::AnimationRef animation = {};
        if (!readRecord(source, *roles, index, record))
            return false;
        const uint8_t role = record[0];
        if (role == 0 ||
            role >= kFirmwarePlaybackRoleCount || role <= previousRole || record[1] != 0 ||
            readU16(record + 6) != 0 ||
            !resolveAnimation(source, *assets, *animations, readU16(record + 2), scope, animation))
            return false;
        previousRole = role;
        rolePresent[role] = true;
        candidate.systemAnimations[role] = animation;
        const uint16_t layoutVersion = readU16(record + 4);
        if (layoutVersion > UINT8_MAX ||
            ((featureFlags & kDynamicActionLayoutFeature) == 0 && layoutVersion != 0))
            return false;
        candidate.actionLayoutVersions[role] = static_cast<uint8_t>(layoutVersion);
    }
    if (!requireRole(rolePresent, FirmwarePlaybackRole::Battery) ||
        ((featureFlags & kPredictFeature) != 0 &&
         (!requireRole(rolePresent, FirmwarePlaybackRole::PredAnim) ||
          !requireRole(rolePresent, FirmwarePlaybackRole::Predict11))) ||
        ((featureFlags & kStartupAnimationFeature) != 0 &&
         (!requireRole(rolePresent, FirmwarePlaybackRole::Start) ||
          !requireRole(rolePresent, FirmwarePlaybackRole::StartIntro))) ||
        ((featureFlags & kFirstStartAnimationFeature) != 0 &&
         !requireRole(rolePresent, FirmwarePlaybackRole::FirstStart)))
        return false;

    if (layouts != nullptr)
    {
        if (layouts->count == 0 || layouts->count > kMaxRuntimeTableLayouts)
            return false;
        for (uint16_t index = 0; index < layouts->count; ++index)
        {
            uint8_t record[12] = {};
            RuntimeTableLayoutConfig &layout = candidate.layouts[index];
            if (!readRecord(source, *layouts, index, record) || readU16(record) != index + 1 ||
                readU16(record + 10) != 0 ||
                !resolveAnimation(source, *assets, *animations, readU16(record + 2), scope, layout.unselected) ||
                !resolveAnimation(source, *assets, *animations, readU16(record + 4), scope, layout.selected))
                return false;
            layout.active = true;
            layout.version = readU16(record);
            layout.x = readI16(record + 6);
            layout.y = readI16(record + 8);
        }
        candidate.layoutCount = static_cast<uint8_t>(layouts->count);
        candidate.layoutUnselected = candidate.layouts[0].unselected;
        candidate.layoutSelected = candidate.layouts[0].selected;
        for (size_t role = 1; role < kFirmwarePlaybackRoleCount; ++role)
            if (candidate.actionLayoutVersions[role] > candidate.layoutCount)
                return false;
    }

    bool flowPresent[3] = {};
    uint16_t expectedChild = 0;
    uint8_t previousFlow = 0;
    if (flows != nullptr)
    {
        for (uint16_t index = 0; index < flows->count; ++index)
        {
            uint8_t record[16] = {};
            if (!readRecord(source, *flows, index, record) || record[0] > 2 ||
                (index != 0 && record[0] <= previousFlow) || record[1] != 0 ||
                readU16(record + 2) != expectedChild || readU16(record + 12) != 0 ||
                readU16(record + 14) != 0 || readU16(record + 10) != 0)
                return false;
            if (record[0] == 1 &&
                (record[6] > 2 || record[7] > 2 || record[8] > 1 || record[9] != 3))
                return false;
            if (record[0] != 1 && (record[6] != 0 || record[7] != 0 ||
                                   record[8] != 0 || record[9] != 0))
                return false;
            previousFlow = record[0];
            flowPresent[record[0]] = true;
            const uint16_t count = readU16(record + 4);
            if (static_cast<uint32_t>(expectedChild) + count > flowRoles->count)
                return false;
            uint8_t previousSlot = 0;
            bool slotSeen[13] = {};
            for (uint16_t child = 0; child < count; ++child)
            {
                uint8_t roleRecord[8] = {};
                if (!readRecord(source, *flowRoles, static_cast<uint16_t>(expectedChild + child), roleRecord) ||
                    roleRecord[0] != record[0] || readU32(roleRecord + 4) != 0 ||
                    roleRecord[2] == 0 || roleRecord[2] >= kFirmwarePlaybackRoleCount ||
                    flowRoleAt(record[0], roleRecord[1]) !=
                        static_cast<FirmwarePlaybackRole>(roleRecord[2]) ||
                    (child != 0 && roleRecord[1] <= previousSlot) ||
                    !rolePresent[roleRecord[2]])
                    return false;
                previousSlot = roleRecord[1];
                slotSeen[roleRecord[1]] = true;
            }
            bool requiredSlot[13] = {};
            if (record[0] == 0)
            {
                for (uint8_t slot = 0; slot < 12; ++slot)
                    requiredSlot[slot] = true;
            }
            else if (record[0] == 2)
            {
                requiredSlot[0] = true;
                requiredSlot[1] = true;
                requiredSlot[2] = (featureFlags & kFirstStartAnimationFeature) != 0;
            }
            else
            {
                requiredSlot[12] = record[6] == 0 || record[6] == 2;
                requiredSlot[4] = requiredSlot[5] = requiredSlot[6] = requiredSlot[11] =
                    record[6] == 1 || record[6] == 2;
                requiredSlot[7] = requiredSlot[10] = true;
                if (record[7] == 0 || record[7] == 2)
                    requiredSlot[0] = requiredSlot[1] = true;
                if (record[7] == 1 || record[7] == 2)
                    requiredSlot[8] = requiredSlot[9] = true;
                if (record[8] == 0)
                    requiredSlot[2] = requiredSlot[3] = true;
            }
            for (uint8_t slot = 0; slot < 13; ++slot)
                if (slotSeen[slot] != requiredSlot[slot])
                    return false;
            expectedChild = static_cast<uint16_t>(expectedChild + count);
        }
        if (expectedChild != flowRoles->count)
            return false;
    }
    if (((featureFlags & kPredictFeature) != 0 && !flowPresent[0]) ||
        ((featureFlags & kGuessGameFeature) != 0 && !flowPresent[1]) ||
        ((featureFlags & kStartupAnimationFeature) != 0 && !flowPresent[2]))
        return false;
    config = candidate;
    return true;
}
} // namespace

bool loadRuntimeManifest(SdFat *sd, AssetData::RuntimeManifest &manifest)
{
    manifest = {};
    if (sd == nullptr)
        return false;
    SdBaseFile file;
    if (!file.open(kRuntimeTablePath, FILE_READ))
        return false;
    const uint32_t byteCount = file.fileSize();
    FileSource fileSource = {&file};
    const Source source = {&fileSource, readFile, byteCount};
    Section sections[kMaxSections] = {};
    uint16_t sectionCount = 0;
    uint32_t featureFlags = 0;
    uint32_t schemaFingerprint = 0;
    AssetData::BundleId bundleId = {};
    const bool decoded = readEnvelope(source, sections, sectionCount,
                                      featureFlags, schemaFingerprint, bundleId);
    file.close();
    if (!decoded)
        return false;
    manifest.bundleId = bundleId;
    return true;
}

bool parseRuntimeTableBehavior(const uint8_t *bytes,
                               size_t byteCount,
                               const AssetData::RuntimeManifest &manifest,
                               uint8_t speciesSlot,
                               uint8_t outfitSlot,
                               PetBehaviorConfig &config)
{
    if (bytes == nullptr || byteCount > UINT32_MAX)
        return false;
    MemorySource memory = {bytes, static_cast<uint32_t>(byteCount)};
    const Source source = {&memory, readMemory, memory.size};
    RuntimeTable table = {};
    return readRuntimeTable(source, &manifest, table) &&
           decodeRuntimeTableBehavior(table, manifest, speciesSlot, outfitSlot, config);
}

bool loadCompleteRuntimeTable(SdFat *sd,
                              const AssetData::RuntimeManifest &manifest,
                              BundleReader &bundleReader,
                              uint8_t speciesSlot,
                              uint8_t outfitSlot,
                              PetBehaviorConfig &config)
{
    if (sd == nullptr)
        return false;
    SdBaseFile file;
    if (!file.open(kRuntimeTablePath, FILE_READ))
        return false;
    const uint32_t byteCount = file.fileSize();
    FileSource fileSource = {&file};
    const Source source = {&fileSource, readFile, byteCount};
    RuntimeTable table = {};

    PetBehaviorConfig candidate = {};
    AppearanceSelection initialAppearance = {};
    AssetData::AnimationRef idleAnimation = {};
    AppearanceQuery query = {};
    query.kind = AppearanceQueryKind::Initial;
    query.selection = &initialAppearance;
    query.idleAnimation = &idleAnimation;
    const bool decoded =
        readRuntimeTable(source, &manifest, table) &&
        decodeRuntimeTableBehavior(table, manifest, speciesSlot, outfitSlot, candidate) &&
        decodeRuntimeTableFlow(table, speciesSlot, outfitSlot, candidate) &&
        decodeRuntimeTableAppearance(table, bundleReader, query);
    file.close();
    if (!decoded)
        return false;
    candidate.idleAnimation = idleAnimation;
    config = candidate;
    return true;
}

namespace
{
bool loadRuntimeTableAppearanceQuery(SdFat *sd,
                                     const AssetData::RuntimeManifest &manifest,
                                     BundleReader &bundleReader,
                                     AppearanceQuery &query)
{
    if (sd == nullptr)
        return false;
    SdBaseFile file;
    if (!file.open(kRuntimeTablePath, FILE_READ))
        return false;
    const uint32_t byteCount = file.fileSize();
    FileSource fileSource = {&file};
    const Source source = {&fileSource, readFile, byteCount};
    RuntimeTable table = {};
    const bool decoded = readRuntimeTable(source, &manifest, table) &&
                         decodeRuntimeTableAppearance(table, bundleReader, query);
    file.close();
    return decoded;
}
} // namespace

bool validateRuntimeTableAppearance(SdFat *sd,
                                    const AssetData::RuntimeManifest &manifest,
                                    BundleReader &bundleReader,
                                    const ActivePetBehaviorStatSlots &activeSlots,
                                    const PetStatSnapshot &stats)
{
    AppearanceQuery query = {};
    query.activeSlots = &activeSlots;
    query.stats = &stats;
    return loadRuntimeTableAppearanceQuery(sd, manifest, bundleReader, query);
}

bool loadRuntimeTableInitialAppearance(SdFat *sd,
                                       const AssetData::RuntimeManifest &manifest,
                                       BundleReader &bundleReader,
                                       AppearanceSelection &selection,
                                       AssetData::AnimationRef *idleAnimation)
{
    selection = {};
    AppearanceQuery query = {};
    query.kind = AppearanceQueryKind::Initial;
    query.selection = &selection;
    query.idleAnimation = idleAnimation;
    return loadRuntimeTableAppearanceQuery(sd, manifest, bundleReader, query);
}

bool findRuntimeTableEvolutionTarget(SdFat *sd,
                                     const AssetData::RuntimeManifest &manifest,
                                     BundleReader &bundleReader,
                                     const ActivePetBehaviorStatSlots &activeSlots,
                                     const PetStatSnapshot &stats,
                                     AppearanceSelection &selection)
{
    selection = {};
    AppearanceQuery query = {};
    query.kind = AppearanceQueryKind::Evolution;
    query.activeSlots = &activeSlots;
    query.stats = &stats;
    query.selection = &selection;
    return loadRuntimeTableAppearanceQuery(sd, manifest, bundleReader, query);
}

bool loadRuntimeTableSpecies(SdFat *sd, const AssetData::RuntimeManifest &manifest,
                             BundleReader &bundleReader, uint8_t *species,
                             size_t maxSpecies, size_t &speciesCount)
{
    speciesCount = 0;
    if (species == nullptr || maxSpecies == 0)
        return false;
    AppearanceQuery query = {};
    query.kind = AppearanceQueryKind::Species;
    query.slots = species;
    query.capacity = maxSpecies;
    const bool loaded = loadRuntimeTableAppearanceQuery(sd, manifest, bundleReader, query);
    speciesCount = query.count;
    return loaded && speciesCount != 0;
}

bool loadRuntimeTableOutfits(SdFat *sd, const AssetData::RuntimeManifest &manifest,
                             BundleReader &bundleReader, uint8_t speciesSlot,
                             uint8_t *outfits, size_t maxOutfits, size_t &outfitCount)
{
    outfitCount = 0;
    if (speciesSlot == 0 || outfits == nullptr || maxOutfits == 0)
        return false;
    AppearanceQuery query = {};
    query.kind = AppearanceQueryKind::Outfits;
    query.speciesSlot = speciesSlot;
    query.slots = outfits;
    query.capacity = maxOutfits;
    const bool loaded = loadRuntimeTableAppearanceQuery(sd, manifest, bundleReader, query);
    outfitCount = query.count;
    return loaded && outfitCount != 0;
}

bool findRuntimeTableOutfitPreview(SdFat *sd, const AssetData::RuntimeManifest &manifest,
                                   BundleReader &bundleReader, uint8_t speciesSlot,
                                   uint8_t outfitSlot, OutfitPreview &preview)
{
    preview = {};
    if (speciesSlot == 0 || outfitSlot == 0)
        return false;
    AppearanceQuery query = {};
    query.kind = AppearanceQueryKind::Preview;
    query.speciesSlot = speciesSlot;
    query.outfitSlot = outfitSlot;
    query.preview = &preview;
    return loadRuntimeTableAppearanceQuery(sd, manifest, bundleReader, query) &&
           preview.animation.valid();
}
