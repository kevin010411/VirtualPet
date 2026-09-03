#include "pet_behavior/domain/RuntimeTableBehavior.h"

#include <limits.h>
#include <string.h>
#include "appearance/domain/RuntimeTableAppearance.h"
#include "commands/domain/SystemCommandCatalog.h"
#include "shared/integrity/Crc32.h"
#include "shared/sd/SdBinaryRead.h"

namespace
{
constexpr char kRuntimeTablePath[] = "/runtime.bin";
constexpr uint8_t kMagic[4] = {'V', 'P', 'R', 'T'};
// Outfit preview ordering is an incompatible runtime contract: v1 must fail
// closed so slot-based persisted selections are never interpreted as v2.
constexpr uint16_t kVersion = 2;
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
    OutfitUnlocks = 35,
    OutfitUnlockConditions = 36,
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
    case OutfitUnlocks: return 8;
    case OutfitUnlockConditions: return 12;
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

bool validateSourceCrc(const Source &source, uint32_t expected)
{
    uint8_t bytes[64] = {};
    uint32_t crc = 0xFFFFFFFFUL;
    for (uint32_t offset = 0; offset < source.size; offset += sizeof(bytes))
    {
        size_t count = sizeof(bytes);
        if (count > source.size - offset)
            count = static_cast<size_t>(source.size - offset);
        if (!source.readAt(source.context, offset, bytes, count))
            return false;
        if (offset == 0)
            memset(bytes + 48, 0, sizeof(uint32_t));
        crc = Integrity::crc32Update(crc, bytes, count);
    }
    return ~crc == expected;
}

bool readEnvelope(const Source &source, Section *sections, uint16_t &sectionCount,
                  uint32_t &featureFlags, uint32_t &schemaFingerprint,
                  AssetData::BundleId &bundleId, uint32_t &fileCrc32)
{
    uint8_t header[kHeaderSize] = {};
    if (source.size < kHeaderSize || source.size > kMaxFileSize ||
        !source.readAt(source.context, 0, header, sizeof(header)) ||
        memcmp(header, kMagic, sizeof(kMagic)) != 0 ||
        readU16(header + 4) != kVersion || readU16(header + 6) != kHeaderSize ||
        readU32(header + 8) != 0x01020304UL || readU32(header + 16) != source.size ||
        readU16(header + 26) != kSectionEntrySize)
        return false;
    for (size_t index = 52; index < sizeof(header); ++index)
        if (header[index] != 0)
            return false;

    featureFlags = readU32(header + 12);
    if ((featureFlags & kPetBehaviorFeature) == 0)
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
    fileCrc32 = readU32(header + 48);
    sectionCount = readU16(header + 24);
    if (sectionCount == 0 || sectionCount > kMaxSections)
        return false;
    const uint32_t directoryBytes = static_cast<uint32_t>(sectionCount) * kSectionEntrySize;
    uint32_t directoryEnd = 0;
    if (!addU32(kHeaderSize, directoryBytes, directoryEnd) || directoryEnd > source.size)
        return false;

    for (uint16_t index = 0; index < sectionCount; ++index)
    {
        uint8_t entry[kSectionEntrySize] = {};
        if (!source.readAt(source.context,
                           kHeaderSize + static_cast<uint32_t>(index) * kSectionEntrySize,
                           entry, sizeof(entry)))
            return false;
        Section &section = sections[index];
        section.type = readU16(entry);
        section.offset = readU32(entry + 4);
        section.count = readU16(entry + 8);
        section.recordSize = readU16(entry + 10);
        section.length = readU32(entry + 12);
        const uint16_t expectedRecordSize = recordSizeFor(section.type);
        const uint32_t expectedLength =
            static_cast<uint32_t>(section.count) * section.recordSize;
        uint32_t end = 0;
        if (expectedRecordSize == 0 ||
            section.recordSize != expectedRecordSize || section.length != expectedLength ||
            !addU32(section.offset, section.length, end) || end > source.size)
            return false;
    }
    return true;
}

bool readRuntimeTable(const Source &source,
                      const AssetData::RuntimeManifest *expectedManifest,
                      RuntimeTable &table)
{
    table = {};
    table.source = source;
    uint32_t fileCrc32 = 0;
    if (!readEnvelope(source, table.sections, table.sectionCount,
                       table.featureFlags, table.schemaFingerprint, table.bundleId, fileCrc32) ||
        (expectedManifest != nullptr &&
         !AssetData::sameBundleId(table.bundleId, expectedManifest->bundleId)))
        return false;
    const bool trustedManifest = expectedManifest != nullptr && expectedManifest->fileSize != 0;
    if (trustedManifest)
        return expectedManifest->fileSize == source.size &&
               expectedManifest->schemaFingerprint == table.schemaFingerprint &&
               expectedManifest->fileCrc32 == fileCrc32;
    return validateSourceCrc(source, fileCrc32);
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
    if (!readRecord(source, animations, animationRef, animation))
        return false;
    const uint16_t runtimeId = readU16(animation + 2);
    const uint16_t firstAsset = readU16(animation + 4);
    const uint16_t assetCount = readU16(animation + 6);
    if (runtimeId == 0 || runtimeId > AssetData::kMaxRuntimeAnimationId ||
        static_cast<uint32_t>(firstAsset) + assetCount > assets.count)
        return false;
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

void clearOwnedBehavior(PetBehaviorConfig &config)
{
    memset(config.stats, 0, sizeof(config.stats));
    memset(config.idleTriggers, 0, sizeof(config.idleTriggers));
    memset(config.actions, 0, sizeof(config.actions));
    memset(config.randomOutcomes, 0, sizeof(config.randomOutcomes));
    memset(config.actionConditions, 0, sizeof(config.actionConditions));
    memset(config.actionEffects, 0, sizeof(config.actionEffects));
    memset(config.randomOutcomeEffects, 0, sizeof(config.randomOutcomeEffects));
#if ENABLE_GUESS_GAME
    memset(config.guessEffects, 0, sizeof(config.guessEffects));
#endif
    memset(config.buttons, 0, sizeof(config.buttons));
    memset(&config.statusSets, 0, sizeof(config.statusSets));
    config.statCount = 0;
    config.idleTriggerCount = 0;
    config.actionCount = 0;
    config.actionConditionCount = 0;
    config.actionEffectCount = 0;
    config.randomOutcomeEffectCount = 0;
#if ENABLE_GUESS_GAME
    config.guessEffectCount = 0;
#endif
    config.buttonCount = 0;
}

bool decodeStats(const Source &source, const Section &section, PetBehaviorConfig &config)
{
    if (section.count == 0 || section.count > kPetBehaviorSlotCount)
        return false;
    for (uint16_t index = 0; index < section.count; ++index)
    {
        uint8_t record[12] = {};
        if (!readRecord(source, section, index, record))
            return false;
        PetBehaviorStatConfig &stat = config.stats[index];
        stat.active = true;
        stat.initialValue = readI16(record + 2);
        stat.minValue = readI16(record + 4);
        stat.maxValue = readI16(record + 6);
        stat.dailyChange = readI16(record + 8);
    }
    config.statCount = static_cast<uint8_t>(section.count);
    return true;
}

bool decodeIdleTriggers(const Source &source,
                        const Section *triggers,
                        const Section &assets,
                        const Section &animations,
                        const ActiveAssetScope &scope,
                        PetBehaviorConfig &config)
{
    if (triggers == nullptr)
        return true;
    if (triggers->count > kMaxPetBehaviorIdleTriggers)
        return false;

    for (uint16_t index = 0; index < triggers->count; ++index)
    {
        uint8_t record[12] = {};
        AssetData::AnimationRef animation = {};
        if (!readRecord(source, *triggers, index, record))
            return false;
        if (record[0] >= config.statCount ||
            !resolveAnimation(source, assets, animations, readU16(record + 4), scope, animation))
            return false;

        PetBehaviorIdleTriggerConfig &trigger = config.idleTriggers[index];
        trigger.active = true;
        trigger.statSlot = record[0];
        trigger.comparison = static_cast<PetBehaviorIdleTriggerOperator>(record[1]);
        trigger.threshold = readI16(record + 2);
        trigger.animation = animation;
    }
    config.idleTriggerCount = static_cast<uint8_t>(triggers->count);
    return true;
}

bool decodeGuessEffects(const Source &source,
                        uint32_t featureFlags,
                        const Section *effects,
                        PetBehaviorConfig &config)
{
    if ((featureFlags & kGuessGameFeature) == 0)
        return effects == nullptr;
#if !ENABLE_GUESS_GAME
    return false;
#else
    if (effects == nullptr)
        return false;
    if (effects->count > kMaxPetBehaviorGuessEffects)
        return false;

    for (uint16_t index = 0; index < effects->count; ++index)
    {
        uint8_t record[8] = {};
        if (!readRecord(source, *effects, index, record) || record[1] >= config.statCount)
            return false;

        PetBehaviorGuessEffectConfig &effect = config.guessEffects[index];
        effect.active = true;
        effect.outcome = static_cast<PetBehaviorGuessOutcome>(record[0]);
        effect.statSlot = record[1];
        effect.operation = static_cast<PetBehaviorEffectOperation>(record[2]);
        effect.value = readI16(record + 4);
    }
    config.guessEffectCount = static_cast<uint8_t>(effects->count);
    return true;
#endif
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
        if (!readRecord(source, actions, actionIndex, actionRecord))
            return false;
        const uint8_t mode = actionRecord[1];
        const uint8_t outcomeCount = actionRecord[6];
        const uint8_t conditionCount = actionRecord[10];
        const bool hasFallback = (actionRecord[3] & 1U) != 0;
        if (outcomeCount > 3 || conditionCount > 4 ||
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
                static_cast<uint32_t>(nextEffect) + readU16(outcomeRecord + 8) >
                    (effects == nullptr ? 0 : effects->count))
                return false;
            const uint8_t weight = outcomeRecord[2];
            const uint8_t playbackCount = outcomeRecord[3];
            const uint16_t animationRef = readU16(outcomeRecord + 4);
            const uint16_t effectCount = readU16(outcomeRecord + 8);
            if (effectCount > kPetBehaviorSlotCount)
                return false;
            AssetData::AnimationRef animation = {};
            const bool missingConditionalFallback = mode == 1 && !hasFallback;
            if (!missingConditionalFallback &&
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

            for (uint16_t effectOffset = 0; effectOffset < effectCount; ++effectOffset)
            {
                uint8_t effectRecord[8] = {};
                if (effects == nullptr || !readRecord(source, *effects, nextEffect, effectRecord) ||
                    effectRecord[2] >= config.statCount)
                    return false;
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

        for (uint8_t conditionSlot = 0; conditionSlot < conditionCount; ++conditionSlot)
        {
            uint8_t conditionRecord[16] = {};
            if (conditions == nullptr || !readRecord(source, *conditions, nextCondition, conditionRecord) ||
                (conditionRecord[2] == 0 && conditionRecord[4] >= config.statCount) ||
                (conditionRecord[2] == 1 && conditionRecord[4] != 0))
                return false;
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
        if (!readRecord(source, buttons, index, record))
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
        if (!readRecord(source, *sets, setIndex, setRecord) ||
            setRecord[1] > kMaxStatusConditions ||
            static_cast<uint32_t>(nextCondition) + setRecord[1] > conditions->count)
            return false;
        StatusSetConfig &set = config.statusSets.sets[setIndex];
        set.conditionCount = setRecord[1];
        if (!resolveAnimation(source, assets, animations, readU16(setRecord + 2),
                              scope, set.animation))
            return false;
        for (uint8_t conditionIndex = 0; conditionIndex < set.conditionCount; ++conditionIndex)
        {
            uint8_t conditionRecord[12] = {};
            if (!readRecord(source, *conditions, nextCondition, conditionRecord) ||
                conditionRecord[1] > 1 ||
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
            ++nextCondition;
        }
    }
    config.statusSets.count = static_cast<uint8_t>(sets->count);
    return nextCondition == conditions->count;
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
    const Section *idleTriggers = table.find(IdleTriggers);
    const Section *actions = table.find(Actions);
    const Section *outcomes = table.find(ActionOutcomes);
    const Section *actionConditions = table.find(ActionConditions);
    const Section *effects = table.find(ActionEffects);
    const Section *guessEffects = table.find(GuessEffects);
    const Section *buttons = table.find(Buttons);
    if (assets == nullptr || animations == nullptr || stats == nullptr || actions == nullptr ||
        outcomes == nullptr || buttons == nullptr)
        return false;

    // The caller discards this configuration when decoding fails.  Decode
    // directly into it to keep the STM32 startup stack bounded.
    config = {};
    clearOwnedBehavior(config);
    config.assetManifest = manifest;
    config.activeSpeciesSlot = speciesSlot;
    config.activeOutfitSlot = outfitSlot;
    config.schemaFingerprint = table.schemaFingerprint;
    const ActiveAssetScope scope = {speciesSlot, outfitSlot};
    if (!decodeStats(source, *stats, config) ||
        !decodeIdleTriggers(source, idleTriggers, *assets, *animations, scope, config) ||
        !decodeActions(source, *actions, *outcomes, actionConditions, effects,
                       *assets, *animations, scope, config) ||
        !decodeGuessEffects(source, table.featureFlags, guessEffects, config) ||
        !decodeButtons(source, *buttons, config) ||
        !decodeStatus(source, table.featureFlags,
                      table.find(StatusSets), table.find(StatusConditions),
                      *assets, *animations, scope, config))
        return false;
    return true;
}

enum class AppearanceQueryKind : uint8_t { Validate, Initial, Evolution, Species, Outfits, Preview, Unlocks };

struct AppearanceQuery
{
    AppearanceQueryKind kind = AppearanceQueryKind::Validate;
    const ActivePetBehaviorStatSlots *activeSlots = nullptr;
    const PetStatSnapshot *stats = nullptr;
    uint8_t speciesSlot = 0;
    uint8_t outfitSlot = 0;
    uint8_t unlockMask = 0;
    bool lockedPreview = false;
    bool initializeUnlockMask = false;
    uint8_t *slots = nullptr;
    size_t capacity = 0;
    size_t count = 0;
    AppearanceSelection *selection = nullptr;
    OutfitPreview *preview = nullptr;
    AssetData::AnimationRef *idleAnimation = nullptr;
    uint8_t *resolvedUnlockMask = nullptr;
};

bool validateAppearanceProjection(const RuntimeTable &table, BundleReader &bundleReader,
                                  const Section &assets, const Section &animations,
                                  const Section &appearance, const Section &species,
                                  const Section &outfits, const Section &unlocks,
                                  const Section &unlockConditions)
{
    const Source &source = table.source;
    const Section *statRecords = table.find(PetStats);
    if (appearance.count != 1 || species.count == 0 || outfits.count == 0 ||
        unlocks.count != outfits.count || statRecords == nullptr)
        return false;
    uint8_t initial[8] = {};
    if (!readRecord(source, appearance, 0, initial) || initial[0] == 0 || initial[1] == 0)
        return false;
    bool initialFound = false;
    for (uint16_t speciesIndex = 0; speciesIndex < species.count; ++speciesIndex)
    {
        uint8_t speciesRecord[8] = {};
        if (!readRecord(source, species, speciesIndex, speciesRecord) ||
            speciesRecord[0] != speciesIndex + 1 || speciesRecord[1] == 0 ||
            static_cast<uint32_t>(readU16(speciesRecord + 2)) + readU16(speciesRecord + 4) > outfits.count)
            return false;
        if (speciesRecord[0] == initial[0] && speciesRecord[1] == initial[1])
            initialFound = true;
        bool entryFound = false;
        for (uint16_t offset = 0; offset < readU16(speciesRecord + 4); ++offset)
        {
            uint8_t outfitRecord[8] = {};
            uint8_t unlockRecord[8] = {};
            const uint16_t outfitIndex = static_cast<uint16_t>(readU16(speciesRecord + 2) + offset);
            AssetData::AnimationRef preview = {};
            if (!readRecord(source, outfits, outfitIndex, outfitRecord) ||
                !readRecord(source, unlocks, outfitIndex, unlockRecord) ||
                outfitRecord[0] != speciesRecord[0] || outfitRecord[1] != offset + 1 ||
                unlockRecord[0] != outfitRecord[0] || unlockRecord[1] != outfitRecord[1] ||
                unlockRecord[2] > 1 || unlockRecord[7] > 4 ||
                unlockRecord[2] != (unlockRecord[7] > 0) ||
                static_cast<uint32_t>(readU16(unlockRecord + 5)) + unlockRecord[7] > unlockConditions.count ||
                (outfitRecord[1] == speciesRecord[1] &&
                 (unlockRecord[2] != 0 || readU16(unlockRecord + 3) != kNone16)) ||
                !resolveAnimation(source, assets, animations, readU16(outfitRecord + 2),
                                  {outfitRecord[0], outfitRecord[1]}, preview) ||
                !AssetData::animationReferenceExists(bundleReader, preview))
                return false;
            uint16_t previousSource = 0;
            for (uint8_t conditionIndex = 0; conditionIndex < unlockRecord[7]; ++conditionIndex)
            {
                uint8_t condition[12] = {};
                if (!readRecord(source, unlockConditions,
                                static_cast<uint16_t>(readU16(unlockRecord + 5) + conditionIndex), condition) ||
                    readU16(condition) != outfitIndex || condition[2] > 1 ||
                    (condition[2] == 1 && condition[3] != 0) ||
                    readI32(condition + 4) > readI32(condition + 8))
                    return false;
                const int32_t minimum = readI32(condition + 4);
                const int32_t maximum = readI32(condition + 8);
                if (condition[2] == 1)
                {
                    if (minimum < 0 || maximum > 3650)
                        return false;
                }
                else
                {
                    uint8_t statRecord[12] = {};
                    if (!readRecord(source, *statRecords, condition[3], statRecord) ||
                        statRecord[0] != condition[3] ||
                        minimum < readI16(statRecord + 4) || maximum > readI16(statRecord + 6))
                        return false;
                }
                const uint16_t normalizedSource = condition[2] == 1
                    ? 1 : static_cast<uint16_t>(2 + condition[3]);
                if (normalizedSource <= previousSource)
                    return false;
                previousSource = normalizedSource;
            }
            const uint16_t lockedPreview = readU16(unlockRecord + 3);
            if (lockedPreview != kNone16 &&
                (!resolveAnimation(source, assets, animations, lockedPreview,
                                   {outfitRecord[0], outfitRecord[1]}, preview) ||
                 !AssetData::animationReferenceExists(bundleReader, preview)))
                return false;
            entryFound = entryFound || outfitRecord[1] == speciesRecord[1];
        }
        if (!entryFound)
            return false;
    }
    return initialFound;
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
    const Section *unlocks = table.find(OutfitUnlocks);
    const Section *unlockConditions = table.find(OutfitUnlockConditions);
    const Section *evolutions = table.find(Evolutions);
    const Section *conditions = table.find(EvolutionConditions);
    if ((featureFlags & (1UL << 2)) == 0 || assets == nullptr || animations == nullptr)
        return false;

    if (query.kind == AppearanceQueryKind::Validate)
    {
        if (appearance == nullptr || species == nullptr || outfits == nullptr ||
            unlocks == nullptr || unlockConditions == nullptr)
            return false;
        return validateAppearanceProjection(table, bundleReader, *assets, *animations,
                                            *appearance, *species, *outfits, *unlocks, *unlockConditions);
    }

    if (query.kind == AppearanceQueryKind::Initial)
    {
        uint8_t record[8] = {};
        AssetData::AnimationRef idle = {};
        if (appearance == nullptr || appearance->count == 0 ||
            !readRecord(source, *appearance, 0, record) ||
            !resolveAnimation(source, *assets, *animations, readU16(record + 2),
                              {record[0], record[1]}, idle) ||
            !AssetData::animationReferenceExists(bundleReader, idle))
            return false;
        query.selection->speciesSlot = record[0];
        query.selection->outfitSlot = record[1];
        if (query.idleAnimation != nullptr)
            *query.idleAnimation = idle;
        return true;
    }

    if (query.kind == AppearanceQueryKind::Species)
    {
        if (species == nullptr)
            return false;
        for (uint16_t index = 0; index < species->count; ++index)
        {
            uint8_t record[8] = {};
            if (query.count >= query.capacity || !readRecord(source, *species, index, record))
                return false;
            query.slots[query.count++] = record[0];
        }
        return true;
    }

    if (query.kind == AppearanceQueryKind::Unlocks)
    {
        if (species == nullptr || outfits == nullptr || unlocks == nullptr ||
            unlockConditions == nullptr || query.resolvedUnlockMask == nullptr ||
            query.stats == nullptr || query.activeSlots == nullptr ||
            query.speciesSlot == 0 || query.speciesSlot > species->count)
            return false;
        uint8_t speciesRecord[8] = {};
        if (!readRecord(source, *species, query.speciesSlot - 1, speciesRecord) ||
            speciesRecord[0] != query.speciesSlot)
            return false;
        uint8_t mask = query.initializeUnlockMask ? 0 : query.unlockMask;
        const uint16_t first = readU16(speciesRecord + 2);
        const uint16_t count = readU16(speciesRecord + 4);
        for (uint16_t offset = 0; offset < count; ++offset)
        {
            const uint16_t index = static_cast<uint16_t>(first + offset);
            uint8_t unlock[8] = {};
            if (!readRecord(source, *unlocks, index, unlock) || unlock[0] != query.speciesSlot ||
                unlock[1] != offset + 1 || unlock[2] > 1 || unlock[7] > 4 ||
                unlock[2] != (unlock[7] > 0))
                return false;
            const bool unconditional = unlock[2] == 0;
            bool matching = !unconditional;
            for (uint8_t conditionIndex = 0; conditionIndex < unlock[7]; ++conditionIndex)
            {
                uint8_t condition[12] = {};
                if (!readRecord(source, *unlockConditions,
                                static_cast<uint16_t>(readU16(unlock + 5) + conditionIndex), condition) ||
                    readU16(condition) != index)
                    return false;
                matching = matching && conditionMatches(condition, *query.stats, *query.activeSlots);
            }
            if ((query.initializeUnlockMask && (unconditional || unlock[1] == speciesRecord[1])) || matching)
                mask |= static_cast<uint8_t>(1U << (unlock[1] - 1U));
        }
        *query.resolvedUnlockMask = mask;
        return true;
    }

    if (query.kind == AppearanceQueryKind::Outfits || query.kind == AppearanceQueryKind::Preview)
    {
        if (outfits == nullptr)
            return false;
        for (uint16_t index = 0; index < outfits->count; ++index)
        {
            uint8_t record[8] = {};
            if (!readRecord(source, *outfits, index, record))
                return false;
            if (record[0] != query.speciesSlot)
                continue;
            uint8_t unlock[8] = {};
            if (unlocks == nullptr || !readRecord(source, *unlocks, index, unlock) ||
                unlock[0] != record[0] || unlock[1] != record[1])
                return false;
            const bool unlocked = (query.unlockMask & (1U << (record[1] - 1U))) != 0;
            if (query.kind == AppearanceQueryKind::Outfits)
            {
                if (!unlocked && readU16(unlock + 3) == kNone16)
                    continue;
                if (query.count >= query.capacity)
                    return false;
                query.slots[query.count++] = record[1];
            }
            else if (record[1] == query.outfitSlot)
            {
                const uint16_t animationRef = query.lockedPreview ? readU16(unlock + 3) : readU16(record + 2);
                if ((!unlocked && !query.lockedPreview) || animationRef == kNone16 ||
                    !resolveAnimation(source, *assets, *animations, animationRef,
                                      {record[0], record[1]}, query.preview->animation) ||
                    !AssetData::animationReferenceExists(bundleReader, query.preview->animation))
                    return false;
                query.preview->speciesSlot = record[0];
                query.preview->outfitSlot = record[1];
                return true;
            }
        }
        return query.kind == AppearanceQueryKind::Outfits;
    }

    if (query.kind == AppearanceQueryKind::Evolution &&
        (featureFlags & (1UL << 3)) != 0)
    {
        if (evolutions == nullptr || conditions == nullptr || query.activeSlots == nullptr ||
            query.stats == nullptr || query.selection == nullptr)
            return false;
        uint16_t nextCondition = 0;
        for (uint16_t index = 0; index < evolutions->count; ++index)
        {
            uint8_t evolution[16] = {};
            if (!readRecord(source, *evolutions, index, evolution) || evolution[5] > 4 ||
                static_cast<uint32_t>(nextCondition) + evolution[5] > conditions->count)
                return false;
            bool matched = query.stats->speciesSlot == evolution[2];
            for (uint8_t offset = 0; offset < evolution[5]; ++offset)
            {
                uint8_t condition[12] = {};
                if (!readRecord(source, *conditions, static_cast<uint16_t>(nextCondition + offset), condition))
                    return false;
                matched = matched && conditionMatches(condition, *query.stats, *query.activeSlots);
            }
            nextCondition = static_cast<uint16_t>(nextCondition + evolution[5]);
            if (matched && query.selection->speciesSlot == 0)
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

// Flow and FlowRoles are export/inspector metadata. Firmware executes the
// resolved system roles and layouts below, without revalidating that catalog.
bool decodeRuntimePresentation(const RuntimeTable &table,
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
    if (assets == nullptr || animations == nullptr || roles == nullptr ||
        ((featureFlags & kDynamicActionLayoutFeature) != 0 && layouts == nullptr) ||
        ((featureFlags & kDynamicActionLayoutFeature) == 0 && layouts != nullptr))
        return false;

    memset(config.systemAnimations, 0, sizeof(config.systemAnimations));
    memset(config.actionLayoutVersions, 0, sizeof(config.actionLayoutVersions));
    memset(config.layouts, 0, sizeof(config.layouts));
    config.layoutUnselected = {};
    config.layoutSelected = {};
    config.layoutCount = 0;
    const ActiveAssetScope scope = {speciesSlot, outfitSlot};
    for (uint16_t index = 0; index < roles->count; ++index)
    {
        uint8_t record[8] = {};
        AssetData::AnimationRef animation = {};
        if (!readRecord(source, *roles, index, record))
            return false;
        const uint8_t role = record[0];
        if (role == 0 || role >= kFirmwarePlaybackRoleCount ||
            !resolveAnimation(source, *assets, *animations, readU16(record + 2), scope, animation))
            return false;
        config.systemAnimations[role] = animation;
        const uint16_t layoutVersion = readU16(record + 4);
        if (layoutVersion > UINT8_MAX)
            return false;
        config.actionLayoutVersions[role] = static_cast<uint8_t>(layoutVersion);
    }

    // Layout and LayoutSel are the base navigation artwork. They are required
    // independently of the optional per-action dynamic Layouts section.
    config.layoutUnselected = config.systemAnimations[
        static_cast<size_t>(FirmwarePlaybackRole::Layout)];
    config.layoutSelected = config.systemAnimations[
        static_cast<size_t>(FirmwarePlaybackRole::LayoutSel)];
    if (!config.layoutUnselected.valid() || !config.layoutSelected.valid())
        return false;

    if (layouts != nullptr)
    {
        if (layouts->count == 0 || layouts->count > kMaxRuntimeTableLayouts)
            return false;
        for (uint16_t index = 0; index < layouts->count; ++index)
        {
            uint8_t record[12] = {};
            RuntimeTableLayoutConfig &layout = config.layouts[index];
            if (!readRecord(source, *layouts, index, record) ||
                !resolveAnimation(source, *assets, *animations, readU16(record + 2), scope, layout.unselected) ||
                !resolveAnimation(source, *assets, *animations, readU16(record + 4), scope, layout.selected))
                return false;
            layout.active = true;
            layout.version = readU16(record);
            layout.x = readI16(record + 6);
            layout.y = readI16(record + 8);
        }
        config.layoutCount = static_cast<uint8_t>(layouts->count);
        for (size_t role = 1; role < kFirmwarePlaybackRoleCount; ++role)
            if (config.actionLayoutVersions[role] > config.layoutCount)
                return false;
    }

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
    uint32_t fileCrc32 = 0;
    AssetData::BundleId bundleId = {};
    const bool decoded = readEnvelope(source, sections, sectionCount,
                                      featureFlags, schemaFingerprint, bundleId, fileCrc32) &&
                         validateSourceCrc(source, fileCrc32);
    file.close();
    if (!decoded)
        return false;
    manifest.bundleId = bundleId;
    manifest.schemaFingerprint = schemaFingerprint;
    manifest.fileSize = byteCount;
    manifest.fileCrc32 = fileCrc32;
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

    AppearanceSelection initialAppearance = {};
    AssetData::AnimationRef idleAnimation = {};
    AppearanceQuery query = {};
    query.kind = AppearanceQueryKind::Initial;
    query.selection = &initialAppearance;
    query.idleAnimation = &idleAnimation;
    const bool decoded =
        readRuntimeTable(source, &manifest, table) &&
        decodeRuntimeTableBehavior(table, manifest, speciesSlot, outfitSlot, config) &&
        decodeRuntimePresentation(table, speciesSlot, outfitSlot, config) &&
        decodeRuntimeTableAppearance(table, bundleReader, query);
    file.close();
    if (!decoded)
        return false;
    config.idleAnimation = idleAnimation;
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
                             uint8_t unlockMask, uint8_t *outfits, size_t maxOutfits, size_t &outfitCount)
{
    outfitCount = 0;
    if (speciesSlot == 0 || outfits == nullptr || maxOutfits == 0)
        return false;
    AppearanceQuery query = {};
    query.kind = AppearanceQueryKind::Outfits;
    query.speciesSlot = speciesSlot;
    query.unlockMask = unlockMask;
    query.slots = outfits;
    query.capacity = maxOutfits;
    const bool loaded = loadRuntimeTableAppearanceQuery(sd, manifest, bundleReader, query);
    outfitCount = query.count;
    return loaded && outfitCount != 0;
}

bool findRuntimeTableOutfitPreview(SdFat *sd, const AssetData::RuntimeManifest &manifest,
                                   BundleReader &bundleReader, uint8_t speciesSlot,
                                   uint8_t outfitSlot, bool locked, OutfitPreview &preview)
{
    preview = {};
    if (speciesSlot == 0 || outfitSlot == 0)
        return false;
    AppearanceQuery query = {};
    query.kind = AppearanceQueryKind::Preview;
    query.speciesSlot = speciesSlot;
    query.outfitSlot = outfitSlot;
    query.lockedPreview = locked;
    query.preview = &preview;
    return loadRuntimeTableAppearanceQuery(sd, manifest, bundleReader, query) &&
           preview.animation.valid();
}

bool resolveRuntimeTableOutfitUnlockMask(SdFat *sd,
                                         const AssetData::RuntimeManifest &manifest,
                                         BundleReader &bundleReader, uint8_t speciesSlot,
                                         const ActivePetBehaviorStatSlots &activeSlots,
                                         const PetStatSnapshot &stats, uint8_t currentMask, bool initialize,
                                         uint8_t &resolvedMask)
{
    resolvedMask = 0;
    if (speciesSlot == 0)
        return false;
    AppearanceQuery query = {};
    query.kind = AppearanceQueryKind::Unlocks;
    query.speciesSlot = speciesSlot;
    query.activeSlots = &activeSlots;
    query.stats = &stats;
    query.unlockMask = currentMask;
    query.initializeUnlockMask = initialize;
    query.resolvedUnlockMask = &resolvedMask;
    return loadRuntimeTableAppearanceQuery(sd, manifest, bundleReader, query);
}
