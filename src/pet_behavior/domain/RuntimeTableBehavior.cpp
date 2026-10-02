#include "pet_behavior/domain/RuntimeTableBehavior.h"

#include <limits.h>
#include <string.h>
#include "appearance/domain/RuntimeTableAppearance.h"
#include "commands/domain/SystemCommandCatalog.h"
#include "pet_behavior/domain/RuntimeValueResolver.h"
#include "shared/sd/SdBinaryRead.h"

namespace
{
constexpr char kRuntimeTablePath[] = "/runtime.bin";
constexpr uint8_t kMagic[4] = {'V', 'P', 'R', 'T'};
// v8 adds the applied screen geometry; .data button-product IDs stay separate.
// Older binaries are rejected without a compatibility parser.
constexpr uint16_t kVersion = 8;
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
constexpr uint32_t kSequentialStatusFeature = 1UL << 9;
constexpr uint32_t kOutfitChooseAnimationFeature = 1UL << 11;
constexpr uint32_t kKnownFeatures = ((1UL << 12) - 1UL) & ~(1UL << 8);

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
    PetStates = 11,
    Actions = 12,
    ActionOutcomes = 13,
    ActionConditions = 14,
    ActionEffects = 15,
    Buttons = 16,
    GuessEffects = 17,
    DefaultPetState = 18,
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
    Flow = 42,
    FlowRoles = 43,
    ScreenBlocks = 50,
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
    case Profile: return 38;
    case Persistence: return 16;
    case AssetRefs: return 12;
    case Animations: return 8;
    case PetStats: return 12;
    case PetStates: return 16;
    case Actions: return 16;
    case ActionOutcomes: return 10;
    case ActionConditions: return 16;
    case ActionEffects: return 6;
    case Buttons: return 8;
    case GuessEffects: return 8;
    case DefaultPetState: return 4;
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
    case Flow: return 16;
    case FlowRoles: return 8;
    case ScreenBlocks: return 16;
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
    if ((featureFlags & kPetBehaviorFeature) == 0 ||
        (featureFlags & ~kKnownFeatures) != 0)
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
    if (expectedManifest == nullptr || expectedManifest->fileSize == 0)
        return false;
    table = {};
    table.source = source;
    uint32_t fileCrc32 = 0;
    if (!readEnvelope(source, table.sections, table.sectionCount,
                       table.featureFlags, table.schemaFingerprint, table.bundleId, fileCrc32) ||
        !AssetData::sameBundleId(table.bundleId, expectedManifest->bundleId))
        return false;
    return expectedManifest->fileSize == source.size &&
           expectedManifest->schemaFingerprint == table.schemaFingerprint &&
           expectedManifest->fileCrc32 == fileCrc32;
}

// Keep the file, its Source context, and the validated table alive together.
// Appearance queries and complete contract loads both use one fresh snapshot.
class RuntimeTableFile
{
public:
    RuntimeTableFile() : fileSource_{&file_} {}
    ~RuntimeTableFile() { file_.close(); }

    RuntimeTableFile(const RuntimeTableFile &) = delete;
    RuntimeTableFile &operator=(const RuntimeTableFile &) = delete;

    bool open(SdFat *sd, const AssetData::RuntimeManifest &manifest)
    {
        if (sd == nullptr || !file_.open(kRuntimeTablePath, FILE_READ))
            return false;
        const Source source = {&fileSource_, readFile, file_.fileSize()};
        return readRuntimeTable(source, &manifest, table_);
    }

    const RuntimeTable &table() const { return table_; }

private:
    SdBaseFile file_;
    FileSource fileSource_;
    RuntimeTable table_ = {};
};

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

bool resolveAssetReference(const Source &source, const Section &assets,
                           uint16_t reference, AssetData::AnimationRef &resolved)
{
    resolved = {};
    if (reference >= assets.count)
        return false;
    uint8_t asset[12] = {};
    if (!readRecord(source, assets, reference, asset) || readU16(asset) != reference)
        return false;
    const uint8_t scope = asset[2];
    if (scope != 1 && scope != 2)
        return false;
    if ((scope == 2 && (asset[3] != 0 || asset[4] != 0)) ||
        (scope == 1 && (asset[3] == 0 || asset[4] == 0)))
        return false;
    resolved.speciesSlot = scope == 2 ? 0 : asset[3];
    resolved.outfitSlot = scope == 2 ? 0 : asset[4];
    resolved.animationId = readU16(asset + 6);
    return resolved.valid();
}

void clearOwnedBehavior(PetBehaviorConfig &config)
{
    memset(config.stats, 0, sizeof(config.stats));
    memset(config.petStates, 0, sizeof(config.petStates));
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
    config.petStateCount = 0;
    config.idleAnimation = {};
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

bool decodePetStates(const Source &source,
                     const Section *states,
                     const Section *defaultState,
                     const Section &assets,
                     const Section &animations,
                     const ActiveAssetScope &scope,
                     PetBehaviorConfig &config)
{
    if (states == nullptr || defaultState == nullptr ||
        states->count > kMaxRuntimePetStates || defaultState->count != 1)
        return false;

    for (uint16_t index = 0; index < states->count; ++index)
    {
        uint8_t record[16] = {};
        AssetData::AnimationRef animation = {};
        if (!readRecord(source, *states, index, record))
            return false;
        const RuntimeRangePredicate predicate = {
            readU16(record + 2), readI32(record + 4), readI32(record + 8)};
        if (readU16(record) != index || readU16(record + 14) != 0 ||
            !isRuntimeBehaviorRange(predicate) ||
            (isRuntimeValueIdPetStat(predicate.valueId) &&
             runtimePetStatSlot(predicate.valueId) >= config.statCount) ||
            !resolveAnimation(source, assets, animations, readU16(record + 12), scope, animation))
            return false;

        RuntimePetStateConfig &state = config.petStates[index];
        state.predicate = predicate;
        state.idleAnimation = animation;
    }

    uint8_t record[4] = {};
    if (!readRecord(source, *defaultState, 0, record) ||
        readU16(record + 2) != 0 ||
        !resolveAnimation(source, assets, animations, readU16(record), scope,
                          config.idleAnimation))
        return false;

    config.petStateCount = static_cast<uint8_t>(states->count);
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
            uint8_t outcomeRecord[10] = {};
            if (!readRecord(source, outcomes, nextOutcome, outcomeRecord) ||
                readU16(outcomeRecord + 4) != nextEffect ||
                static_cast<uint32_t>(nextEffect) + readU16(outcomeRecord + 6) >
                    (effects == nullptr ? 0 : effects->count) ||
                readU16(outcomeRecord + 8) != 0)
                return false;
            const uint8_t weight = outcomeRecord[0];
            const uint8_t playbackCount = outcomeRecord[1];
            const uint16_t animationRef = readU16(outcomeRecord + 2);
            const uint16_t effectCount = readU16(outcomeRecord + 6);
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
                uint8_t effectRecord[6] = {};
                if (effects == nullptr || !readRecord(source, *effects, nextEffect, effectRecord) ||
                    effectRecord[0] >= config.statCount || effectRecord[1] > 1 ||
                    readU16(effectRecord + 4) != 0)
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
                    effect.statSlot = effectRecord[0];
                    effect.operation = static_cast<PetBehaviorEffectOperation>(effectRecord[1]);
                    effect.value = readI16(effectRecord + 2);
                }
                else
                {
                    if (config.actionEffectCount >= kMaxPetBehaviorActionEffects)
                        return false;
                    PetBehaviorActionEffectConfig &effect =
                        config.actionEffects[config.actionEffectCount++];
                    effect.active = true;
                    effect.actionSlot = static_cast<uint8_t>(actionIndex);
                    effect.statSlot = effectRecord[0];
                    effect.operation = static_cast<PetBehaviorEffectOperation>(effectRecord[1]);
                    effect.value = readI16(effectRecord + 2);
                }
                ++nextEffect;
            }
            ++nextOutcome;
        }

        for (uint8_t conditionSlot = 0; conditionSlot < conditionCount; ++conditionSlot)
        {
            uint8_t conditionRecord[16] = {};
            if (conditions == nullptr ||
                !readRecord(source, *conditions, nextCondition, conditionRecord))
                return false;
            const RuntimeRangePredicate predicate = {
                readU16(conditionRecord + 2), readI32(conditionRecord + 8),
                readI32(conditionRecord + 12)};
            if (conditionRecord[1] != 0 || conditionRecord[5] != 0 ||
                conditionRecord[4] == 0 || conditionRecord[4] > 5 ||
                !isRuntimeBehaviorRange(predicate) ||
                (isRuntimeValueIdPetStat(predicate.valueId) &&
                 runtimePetStatSlot(predicate.valueId) >= config.statCount))
                return false;
            PetBehaviorActionConditionConfig &condition =
                config.actionConditions[config.actionConditionCount++];
            condition.active = true;
            condition.actionSlot = static_cast<uint8_t>(actionIndex);
            condition.priority = conditionRecord[0];
            condition.predicate = predicate;
            condition.animationPlayback.playbackCount = conditionRecord[4];
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
            record[1] > static_cast<uint8_t>(PetBehaviorButtonKind::SystemCommand) ||
            readU32(record + 4) != 0)
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
            setRecord[0] != setIndex ||
            setRecord[1] > kMaxStatusConditions ||
            readU16(setRecord + 4) != nextCondition ||
            static_cast<uint32_t>(nextCondition) + setRecord[1] > conditions->count)
            return false;
        StatusSetConfig &set = config.statusSets.sets[setIndex];
        set.conditionCount = setRecord[1];
        set.versionCount = readU16(setRecord + 6);
        if (set.versionCount == 0 || set.versionCount > AssetData::kMaxVersions ||
            setRecord[8] != 1 ||
            setRecord[9] != 0 || readU16(setRecord + 10) != 0)
            return false;
        if (!resolveAnimation(source, assets, animations, readU16(setRecord + 2),
                              scope, set.animation))
            return false;
        uint16_t versionProduct = 1;
        for (uint8_t conditionIndex = 0; conditionIndex < set.conditionCount; ++conditionIndex)
        {
            uint8_t conditionRecord[12] = {};
            if (!readRecord(source, *conditions, nextCondition, conditionRecord) ||
                conditionRecord[3] == 0 || conditionRecord[3] > 32 ||
                conditionRecord[0] > 1 ||
                readI32(conditionRecord + 4) > readI32(conditionRecord + 8))
                return false;
            StatusSetCondition &condition = set.conditions[conditionIndex];
            condition.kind = static_cast<StatusConditionKind>(conditionRecord[0]);
            const uint16_t sourceValue = readU16(conditionRecord + 1);
            condition.valueId = condition.kind == StatusConditionKind::PetStatusAxis
                                    ? 0
                                    : sourceValue;
            condition.petStateMask = condition.kind == StatusConditionKind::PetStatusAxis
                                         ? sourceValue
                                         : 0;
            condition.levels = conditionRecord[3];
            condition.minValue = readI32(conditionRecord + 4);
            condition.maxValue = readI32(conditionRecord + 8);
            if (condition.kind == StatusConditionKind::PetStatusAxis)
            {
                const uint16_t allowedMask = config.petStateCount >= 16
                                                   ? 0xFFFFU
                                                   : static_cast<uint16_t>((1U << config.petStateCount) - 1U);
                const uint16_t selectedMask = condition.petStateMask;
                uint8_t selectedCount = 0;
                for (uint16_t mask = selectedMask; mask != 0; mask >>= 1)
                    selectedCount = static_cast<uint8_t>(selectedCount + (mask & 1U));
                if (selectedMask == 0 || (selectedMask & ~allowedMask) != 0 ||
                    condition.levels != static_cast<uint8_t>(selectedCount + 1))
                    return false;
            }
            else
            {
                const RuntimeRangePredicate predicate = {
                    condition.valueId, condition.minValue, condition.maxValue};
                if (!isRuntimeBehaviorRange(predicate) ||
                    (isRuntimeValueIdPetStat(condition.valueId) &&
                     runtimePetStatSlot(condition.valueId) >= config.statCount))
                    return false;
            }
            versionProduct = static_cast<uint16_t>(
                versionProduct * condition.levels);
            if (versionProduct > AssetData::kMaxVersions)
                return false;
            ++nextCondition;
        }
        if (versionProduct != set.versionCount)
            return false;
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
    const Section *petStates = table.find(PetStates);
    const Section *defaultPetState = table.find(DefaultPetState);
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
        !decodePetStates(source, petStates, defaultPetState,
                         *assets, *animations, scope, config) ||
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

struct EvolutionRecord
{
    uint8_t sourceSpecies = 0;
    uint8_t targetSpecies = 0;
    uint8_t targetOutfit = 0;
    uint8_t conditionCount = 0;
    uint16_t sourceAnimationRef = kNone16;
    uint16_t targetAnimationRef = kNone16;
    EvolutionAnimationMode mode = EvolutionAnimationMode::Disabled;
};

bool readEvolutionRecord(const Source &source, const Section &evolutions,
                         const Section *conditions, const Section &animations,
                         uint16_t index, uint16_t expectedFirstCondition,
                         EvolutionRecord &decoded)
{
    uint8_t record[16] = {};
    if (!readRecord(source, evolutions, index, record))
        return false;
    decoded.sourceSpecies = record[2];
    decoded.targetSpecies = record[3];
    decoded.targetOutfit = record[4];
    decoded.conditionCount = record[5];
    decoded.sourceAnimationRef = readU16(record + 8);
    decoded.targetAnimationRef = readU16(record + 10);
    decoded.mode = static_cast<EvolutionAnimationMode>(record[12]);
    const bool referencesMatchMode =
        (decoded.mode == EvolutionAnimationMode::Disabled &&
         decoded.sourceAnimationRef == kNone16 && decoded.targetAnimationRef == kNone16) ||
        (decoded.mode == EvolutionAnimationMode::Single &&
         decoded.sourceAnimationRef != kNone16 && decoded.targetAnimationRef == kNone16) ||
        (decoded.mode == EvolutionAnimationMode::TwoPhase &&
         decoded.sourceAnimationRef != kNone16 && decoded.targetAnimationRef != kNone16);
    return decoded.conditionCount <= 4 && readU16(record + 6) == expectedFirstCondition &&
           static_cast<uint32_t>(expectedFirstCondition) + decoded.conditionCount <=
               (conditions == nullptr ? 0 : conditions->count) &&
           (decoded.sourceAnimationRef == kNone16 || decoded.sourceAnimationRef < animations.count) &&
           (decoded.targetAnimationRef == kNone16 || decoded.targetAnimationRef < animations.count) &&
           referencesMatchMode && record[13] == 0 && record[14] == 0 && record[15] == 0;
}

RuntimeValueContext runtimeValueContext(const PetStatSnapshot &stats,
                                        const ActivePetBehaviorStatSlots &activeSlots)
{
    RuntimeValueContext context = {};
    context.petStats = stats.customStats;
    context.activePetStatMask = activeSlots.mask();
    context.stageDays = stats.stage_days;
    context.speciesSlot = stats.speciesSlot;
    context.outfitSlot = stats.outfitSlot;
    return context;
}

bool readRuntimePredicate(const uint8_t *record, bool allowAppearanceValues,
                          RuntimeRangePredicate &predicate)
{
    predicate = {
        readU16(record), readI32(record + 2), readI32(record + 6)};
    return readU16(record + 10) == 0 &&
           (allowAppearanceValues ? isRuntimeRangeWellFormed(predicate)
                                  : isRuntimeBehaviorRange(predicate));
}

struct SpeciesOutfitRange
{
    uint16_t first;
    uint16_t count;
    uint8_t entryOutfit;
};

bool outfitRangeForSpecies(const RuntimeTable &table, uint8_t speciesSlot,
                            SpeciesOutfitRange &range)
{
    const Section *species = table.find(Species);
    const Section *outfits = table.find(Outfits);
    if (species == nullptr || outfits == nullptr || speciesSlot == 0 ||
        speciesSlot > species->count)
        return false;
    uint8_t record[8] = {};
    if (!readRecord(table.source, *species, speciesSlot - 1U, record) ||
        record[0] != speciesSlot)
        return false;
    range.first = readU16(record + 2);
    range.count = readU16(record + 4);
    range.entryOutfit = record[1];
    return static_cast<uint32_t>(range.first) + range.count <= outfits->count;
}

bool decodeEvolutionQuery(const RuntimeTable &table, BundleReader &bundleReader,
                          const ActivePetBehaviorStatSlots &activeSlots,
                          const PetStatSnapshot &stats, AppearanceSelection &selection)
{
    const Source &source = table.source;
    const Section *assets = table.find(AssetRefs);
    const Section *animations = table.find(Animations);
    const Section *species = table.find(Species);
    const Section *outfits = table.find(Outfits);
    const Section *evolutions = table.find(Evolutions);
    const Section *conditions = table.find(EvolutionConditions);
    if (evolutions == nullptr)
        return false;
    uint16_t nextCondition = 0;
    for (uint16_t index = 0; index < evolutions->count; ++index)
    {
        EvolutionRecord evolution = {};
        if (!readEvolutionRecord(source, *evolutions, conditions, *animations,
                                 index, nextCondition, evolution))
            return false;
        bool matched = stats.speciesSlot == evolution.sourceSpecies;
        for (uint8_t offset = 0; offset < evolution.conditionCount; ++offset)
        {
            uint8_t condition[12] = {};
            RuntimeRangePredicate predicate = {};
            if (conditions == nullptr ||
                !readRecord(source, *conditions, static_cast<uint16_t>(nextCondition + offset), condition) ||
                !readRuntimePredicate(condition, true, predicate))
                return false;
            matched = matched && matchesRuntimeRange(
                                     predicate,
                                     runtimeValueContext(stats, activeSlots));
        }
        nextCondition = static_cast<uint16_t>(nextCondition + evolution.conditionCount);
        if (matched && selection.speciesSlot == 0)
        {
            if (species == nullptr || outfits == nullptr ||
                evolution.targetSpecies == 0 ||
                evolution.targetSpecies > species->count ||
                evolution.targetOutfit == 0 ||
                evolution.targetOutfit > AssetData::kMaxOutfitsPerSpecies)
                return false;
            uint8_t targetSpecies[8] = {};
            uint8_t targetOutfit[8] = {};
            if (!readRecord(source, *species, evolution.targetSpecies - 1,
                            targetSpecies) ||
                evolution.targetOutfit > readU16(targetSpecies + 4))
                return false;
            const uint32_t targetOutfitIndex =
                static_cast<uint32_t>(readU16(targetSpecies + 2)) +
                evolution.targetOutfit - 1U;
            if (targetOutfitIndex >= outfits->count ||
                !readRecord(source, *outfits,
                            static_cast<uint16_t>(targetOutfitIndex), targetOutfit) ||
                targetOutfit[0] != evolution.targetSpecies ||
                targetOutfit[1] != evolution.targetOutfit)
                return false;
            const ActiveAssetScope scope = {stats.speciesSlot, stats.outfitSlot};
            AssetData::AnimationRef sourceAnimation = {};
            AssetData::AnimationRef targetAnimation = {};
            if (evolution.sourceAnimationRef != kNone16 &&
                (!resolveAnimation(source, *assets, *animations, evolution.sourceAnimationRef, scope, sourceAnimation) ||
                 !AssetData::animationReferenceExists(bundleReader, sourceAnimation)))
                return false;
            if (evolution.targetAnimationRef != kNone16 &&
                (!resolveAnimation(source, *assets, *animations, evolution.targetAnimationRef,
                                   {evolution.targetSpecies, evolution.targetOutfit}, targetAnimation) ||
                 !AssetData::animationReferenceExists(bundleReader, targetAnimation)))
                return false;
            selection.speciesSlot = evolution.targetSpecies;
            selection.outfitSlot = evolution.targetOutfit;
            selection.evolutionMode = evolution.mode;
            selection.sourceEvolutionAnimation = sourceAnimation;
            selection.targetEvolutionAnimation = targetAnimation;
        }
    }
    return true;
}

bool readOutfitPair(const RuntimeTable &table, uint16_t index,
                    uint8_t speciesSlot, uint8_t outfitSlot,
                    uint8_t *outfit, uint8_t *unlock)
{
    const Section *outfits = table.find(Outfits);
    const Section *unlocks = table.find(OutfitUnlocks);
    return outfits != nullptr && unlocks != nullptr &&
           readRecord(table.source, *outfits, index, outfit) &&
           readRecord(table.source, *unlocks, index, unlock) &&
           outfit[0] == speciesSlot && outfit[1] == outfitSlot &&
           unlock[0] == speciesSlot && unlock[1] == outfitSlot;
}

bool decodeOutfitList(const RuntimeTable &table, uint8_t speciesSlot,
                      uint8_t unlockMask, uint8_t *slots,
                      size_t capacity, size_t &count)
{
    SpeciesOutfitRange range = {};
    if (!outfitRangeForSpecies(table, speciesSlot, range))
        return false;
    for (uint16_t offset = 0; offset < range.count; ++offset)
    {
        if (offset >= AssetData::kMaxOutfitsPerSpecies)
            return false;
        const uint16_t index = static_cast<uint16_t>(range.first + offset);
        uint8_t outfit[8] = {};
        uint8_t unlock[8] = {};
        if (!readOutfitPair(table, index, speciesSlot,
                            static_cast<uint8_t>(offset + 1U), outfit, unlock))
            return false;
        const bool unlocked = (unlockMask & (1U << (outfit[1] - 1U))) != 0;
        if (!unlocked && readU16(unlock + 3) == kNone16)
            continue;
        if (count >= capacity)
            return false;
        slots[count++] = outfit[1];
    }
    return true;
}

bool decodeOutfitPreview(const RuntimeTable &table, BundleReader &bundleReader,
                         uint8_t speciesSlot, uint8_t outfitSlot, bool locked,
                         OutfitPreview &preview)
{
    SpeciesOutfitRange range = {};
    if (!outfitRangeForSpecies(table, speciesSlot, range) ||
        outfitSlot == 0 || outfitSlot > range.count)
        return false;
    const uint16_t index = static_cast<uint16_t>(range.first + outfitSlot - 1U);
    uint8_t outfit[8] = {};
    uint8_t unlock[8] = {};
    if (!readOutfitPair(table, index, speciesSlot, outfitSlot, outfit, unlock))
        return false;
    const uint16_t animationRef = locked ? readU16(unlock + 3) : readU16(outfit + 2);
    const Section *assets = table.find(AssetRefs);
    const Section *animations = table.find(Animations);
    if (animationRef == kNone16 ||
        !resolveAnimation(table.source, *assets, *animations, animationRef,
                          {speciesSlot, outfitSlot}, preview.animation) ||
        !AssetData::animationReferenceExists(bundleReader, preview.animation))
        return false;
    preview.speciesSlot = speciesSlot;
    preview.outfitSlot = outfitSlot;
    return true;
}

bool decodeOutfitUnlocks(const RuntimeTable &table, uint8_t speciesSlot,
                         const ActivePetBehaviorStatSlots &activeSlots,
                         const PetStatSnapshot &stats, uint8_t currentMask,
                         bool initialize, uint8_t &resolvedMask)
{
    const Source &source = table.source;
    const Section *species = table.find(Species);
    const Section *outfits = table.find(Outfits);
    const Section *unlocks = table.find(OutfitUnlocks);
    const Section *unlockConditions = table.find(OutfitUnlockConditions);
    if (species == nullptr || outfits == nullptr || unlocks == nullptr ||
        unlockConditions == nullptr || speciesSlot == 0 ||
        speciesSlot > species->count)
        return false;
    SpeciesOutfitRange range = {};
    if (!outfitRangeForSpecies(table, speciesSlot, range))
        return false;
    uint8_t mask = initialize ? 0 : currentMask;
    for (uint16_t offset = 0; offset < range.count; ++offset)
    {
        const uint16_t index = static_cast<uint16_t>(range.first + offset);
        uint8_t unlock[8] = {};
        if (!readRecord(source, *unlocks, index, unlock) || unlock[0] != speciesSlot ||
            unlock[1] != offset + 1 || unlock[1] == 0 ||
            unlock[1] > AssetData::kMaxOutfitsPerSpecies ||
            unlock[2] > 2 || unlock[7] > 4 ||
            ((unlock[2] == 0) != (unlock[7] == 0)))
            return false;
        const bool unconditional = unlock[2] == 0;
        bool matching = unlock[2] == 1;
        for (uint8_t conditionIndex = 0; conditionIndex < unlock[7]; ++conditionIndex)
        {
            uint8_t condition[12] = {};
            RuntimeRangePredicate predicate = {};
            if (!readRecord(source, *unlockConditions,
                            static_cast<uint16_t>(readU16(unlock + 5) + conditionIndex), condition) ||
                !readRuntimePredicate(condition, false, predicate))
                return false;
            matching = matching && matchesRuntimeRange(
                                       predicate,
                                       runtimeValueContext(stats, activeSlots));
        }
        if ((initialize && (unconditional || unlock[1] == range.entryOutfit)) || matching)
            mask |= static_cast<uint8_t>(1U << (unlock[1] - 1U));
    }
    resolvedMask = mask;
    return true;
}

bool decodeConsumableOutfitUnlock(const RuntimeTable &table,
                                  uint8_t speciesSlot, uint8_t outfitSlot,
                                  const ActivePetBehaviorStatSlots &activeSlots,
                                  const PetStatSnapshot &stats,
                                  PetStatSnapshot &consumedStats)
{
    const Source &source = table.source;
    const Section *statRecords = table.find(PetStats);
    const Section *outfits = table.find(Outfits);
    const Section *unlocks = table.find(OutfitUnlocks);
    const Section *unlockConditions = table.find(OutfitUnlockConditions);
    if (outfits == nullptr || unlocks == nullptr || unlockConditions == nullptr ||
        statRecords == nullptr || speciesSlot == 0 || outfitSlot == 0)
        return false;
    SpeciesOutfitRange range = {};
    if (!outfitRangeForSpecies(table, speciesSlot, range) ||
        outfitSlot > range.count)
        return false;
    const uint16_t index = static_cast<uint16_t>(range.first + outfitSlot - 1U);
    uint8_t outfit[8] = {};
    uint8_t unlock[8] = {};
    if (!readOutfitPair(table, index, speciesSlot, outfitSlot, outfit, unlock) ||
        unlock[1] == 0 || unlock[1] > AssetData::kMaxOutfitsPerSpecies ||
        unlock[2] != 2 || unlock[7] == 0 || unlock[7] > 4)
        return false;
    PetStatSnapshot result = stats;
    for (uint8_t offset = 0; offset < unlock[7]; ++offset)
    {
        uint8_t condition[12] = {};
        RuntimeRangePredicate predicate = {};
        if (!readRecord(source, *unlockConditions,
                        static_cast<uint16_t>(readU16(unlock + 5) + offset), condition) ||
            !readRuntimePredicate(condition, false, predicate) ||
            !matchesRuntimeRange(
                predicate, runtimeValueContext(stats, activeSlots)))
            return false;
        const RuntimeValueId valueId = readU16(condition);
        const int32_t cost = readI32(condition + 2);
        if (cost < 0)
            return false;
        if (valueId == kRuntimeValueStageDays)
        {
            if (static_cast<uint32_t>(cost) > result.stage_days)
                return false;
            result.stage_days -= static_cast<uint32_t>(cost);
        }
        else if (isRuntimeValueIdPetStat(valueId))
        {
            const uint8_t slot = runtimePetStatSlot(valueId);
            uint8_t stat[12] = {};
            if (slot >= PetStatSnapshot::kCustomStatCount ||
                !activeSlots.contains(slot) ||
                !readRecord(source, *statRecords, slot, stat))
                return false;
            const int32_t next = static_cast<int32_t>(result.customStats[slot]) - cost;
            if (next < readI16(stat + 4) || next > readI16(stat + 6))
                return false;
            result.customStats[slot] = static_cast<int16_t>(next);
        }
        else
            return false;
    }
    consumedStats = result;
    return true;
}

bool decodeInitialAppearance(const RuntimeTable &table, BundleReader &bundleReader,
                             AppearanceSelection &selection,
                             AssetData::AnimationRef *idleAnimation = nullptr)
{
    const Source &source = table.source;
    const Section *assets = table.find(AssetRefs);
    const Section *animations = table.find(Animations);
    const Section *appearance = table.find(Appearance);
    uint8_t record[8] = {};
    AssetData::AnimationRef idle = {};
    if (appearance == nullptr || appearance->count == 0 ||
        !readRecord(source, *appearance, 0, record) ||
        !resolveAnimation(source, *assets, *animations, readU16(record + 2),
                          {record[0], record[1]}, idle) ||
        !AssetData::animationReferenceExists(bundleReader, idle))
        return false;
    selection.speciesSlot = record[0];
    selection.outfitSlot = record[1];
    if (idleAnimation != nullptr)
        *idleAnimation = idle;
    return true;
}

bool decodeSpeciesQuery(const RuntimeTable &table, uint8_t *slots,
                        size_t capacity, size_t &count)
{
    const Section *species = table.find(Species);
    if (species == nullptr)
        return false;
    for (uint16_t index = 0; index < species->count; ++index)
    {
        uint8_t record[8] = {};
        if (count >= capacity ||
            !readRecord(table.source, *species, index, record))
            return false;
        slots[count++] = record[0];
    }
    return true;
}

bool appearanceReferencesAvailable(const RuntimeTable &table)
{
    return (table.featureFlags & (1UL << 2)) != 0 &&
           table.find(AssetRefs) != nullptr && table.find(Animations) != nullptr;
}

bool validateAppearanceSections(const RuntimeTable &table)
{
    if (!appearanceReferencesAvailable(table) ||
        table.find(Appearance) == nullptr || table.find(Species) == nullptr ||
        table.find(Outfits) == nullptr || table.find(OutfitUnlocks) == nullptr)
        return false;
    return (table.featureFlags & (1UL << 3)) == 0 ||
           table.find(Evolutions) != nullptr;
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
#if !ENABLE_OUTFIT_CHOOSE_ANIMATION
    if ((flags & kOutfitChooseAnimationFeature) != 0)
        return false;
#endif
    return (flags & kFirstStartAnimationFeature) == 0 ||
           (flags & kStartupAnimationFeature) != 0;
}

// Flow and FlowRoles are export/inspector metadata. Firmware executes the
// resolved system roles, including the two shared button-layout assets.
bool decodeScreenBlocks(const RuntimeTable &table, PetBehaviorConfig &config)
{
    const Section *blocks = table.find(ScreenBlocks);
    if (blocks == nullptr || blocks->count == 0 || blocks->count > kMaxScreenBlocks)
        return false;
    config.screenBlockCount = 0;
    uint8_t animationCount = 0;
    for (uint16_t index = 0; index < blocks->count; ++index)
    {
        uint8_t record[16] = {};
        if (!readRecord(table.source, *blocks, index, record) ||
            readU16(record + 6) != 0 || readU16(record + 8) != 0 ||
            readU16(record + 10) != 0 || readU32(record + 12) != 0)
            return false;
        ScreenBlockConfig &block = config.screenBlocks[index];
        block = {static_cast<ScreenBlockKind>(record[0]), record[1],
                 record[2], record[3], record[4], record[5]};
        if (block.width == 0 || block.height == 0 ||
            static_cast<uint16_t>(block.x) + block.width > 128 ||
            static_cast<uint16_t>(block.y) + block.height > 160 ||
            (block.x | block.y | block.width | block.height) % 16 != 0)
            return false;
        if (block.kind == ScreenBlockKind::Animation)
        {
            if (block.source != 0 || ++animationCount > 1)
                return false;
        }
        else if (block.kind == ScreenBlockKind::Button)
        {
            if (block.source == 0 || block.source > kPetBehaviorButtonCount)
                return false;
        }
        else
            return false;
        for (uint16_t previous = 0; previous < index; ++previous)
        {
            const ScreenBlockConfig &other = config.screenBlocks[previous];
            if (block.x < other.x + other.width && block.x + block.width > other.x &&
                block.y < other.y + other.height && block.y + block.height > other.y)
                return false;
        }
        // Ticket 01 only enables the canonical default geometry. The record
        // language has capacity for later tickets, but cannot enable them yet.
        if (index == 0)
        {
            if (block.kind != ScreenBlockKind::Animation || block.x != 0 ||
                block.y != 32 || block.width != 128 || block.height != 96)
                return false;
        }
        else if (block.kind != ScreenBlockKind::Button || block.source != index ||
                 block.x != ((index - 1) % 4) * 32 ||
                 block.y != (index <= 4 ? 0 : 128) ||
                 block.width != 32 || block.height != 32)
            return false;
    }
    if (blocks->count != 9 || animationCount != 1)
        return false;
    // Publish the count only after the entire section has passed validation.
    config.screenBlockCount = static_cast<uint8_t>(blocks->count);
    return true;
}

bool decodeRuntimePresentation(const RuntimeTable &table,
                               uint8_t speciesSlot,
                               uint8_t outfitSlot,
                               PetBehaviorConfig &config)
{
    if (speciesSlot == 0 || outfitSlot == 0)
        return false;
    const Source &source = table.source;
    const uint32_t featureFlags = table.featureFlags;
    if (!compiledFeaturesAccept(featureFlags) || !decodeScreenBlocks(table, config))
        return false;

    const Section *assets = table.find(AssetRefs);
    const Section *animations = table.find(Animations);
    const Section *roles = table.find(SystemRoles);
    if (assets == nullptr || animations == nullptr || roles == nullptr)
        return false;

    memset(config.systemAnimations, 0, sizeof(config.systemAnimations));
    config.layoutUnselected = {};
    config.layoutSelected = {};
    const ActiveAssetScope scope = {speciesSlot, outfitSlot};
    if (roles != nullptr)
    {
        for (uint16_t index = 0; index < roles->count; ++index)
        {
            uint8_t record[8] = {};
            AssetData::AnimationRef animation = {};
            if (!readRecord(source, *roles, index, record))
                return false;
            const uint8_t role = record[0];
            if (role == 0 || role >= kFirmwarePlaybackRoleCount ||
                readU16(record + 4) != 0)
                return false;
            if (!resolveAnimation(source, *assets, *animations, readU16(record + 2), scope,
                                  animation))
            {
                // Evolution uses references on each evolution record. Start
                // belongs to the initial appearance, not later species.
                if (role == static_cast<uint8_t>(FirmwarePlaybackRole::Evolution) ||
                    role == static_cast<uint8_t>(FirmwarePlaybackRole::Start))
                    continue;
                return false;
            }
            config.systemAnimations[role] = animation;
            if (role == static_cast<uint8_t>(FirmwarePlaybackRole::Layout))
                config.layoutUnselected = animation;
            else if (role == static_cast<uint8_t>(FirmwarePlaybackRole::LayoutSel))
                config.layoutSelected = animation;
        }
    }
    return config.layoutUnselected.valid() && config.layoutUnselected.shared() &&
           config.layoutSelected.valid() && config.layoutSelected.shared();
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
                                      featureFlags, schemaFingerprint, bundleId, fileCrc32);
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
    PetBehaviorConfig candidate = {};
    if (!readRuntimeTable(source, &manifest, table) ||
        !decodeRuntimeTableBehavior(table, manifest, speciesSlot, outfitSlot, candidate) ||
        !decodeRuntimePresentation(table, speciesSlot, outfitSlot, candidate))
        return false;
    config = candidate;
    return true;
}

bool loadCompleteRuntimeTable(SdFat *sd,
                              const AssetData::RuntimeManifest &manifest,
                              BundleReader &bundleReader,
                              uint8_t speciesSlot,
                              uint8_t outfitSlot,
                              PetBehaviorConfig &config,
                              AppearanceSelection *initialAppearance,
                              bool *initialAppearanceResolved)
{
    // This runs on a 20 KiB SRAM target. PetBehaviorConfig is about 6 KiB, so
    // keeping a second candidate on the stack collides with the heap during
    // startup and faults before the TFT error path can run. Runtime-contract
    // failures are fatal to the active session, so decode into the caller-owned
    // buffer after clearing it instead of preserving an unusable old contract.
    config = {};
    if (initialAppearance != nullptr)
        *initialAppearance = {};
    if (initialAppearanceResolved != nullptr)
        *initialAppearanceResolved = false;
    RuntimeTableFile tableFile;
    if (!tableFile.open(sd, manifest))
        return false;
    const RuntimeTable &table = tableFile.table();

    AppearanceSelection decodedInitial = {};
    bool decoded = true;
    if (initialAppearance != nullptr)
    {
        decoded = appearanceReferencesAvailable(table) &&
                  decodeInitialAppearance(table, bundleReader, decodedInitial);
        if (decoded)
        {
            speciesSlot = decodedInitial.speciesSlot;
            outfitSlot = decodedInitial.outfitSlot;
            if (initialAppearanceResolved != nullptr)
                *initialAppearanceResolved = true;
        }
    }
    decoded = decoded &&
              decodeRuntimeTableBehavior(table, manifest, speciesSlot, outfitSlot, config) &&
              decodeRuntimePresentation(table, speciesSlot, outfitSlot, config);
    if (decoded && initialAppearance == nullptr)
        decoded = appearanceReferencesAvailable(table) &&
                  decodeInitialAppearance(table, bundleReader, decodedInitial);
    if (decoded)
        decoded = AssetData::animationReferenceExists(bundleReader, config.idleAnimation);
    // The former second /runtime.bin read only checked these required
    // appearance sections. Keep that check after used asset references.
    if (decoded)
        decoded = validateAppearanceSections(table);
    if (!decoded)
        return false;
    if (initialAppearance != nullptr)
        *initialAppearance = decodedInitial;
    return true;
}

namespace
{
bool openAppearanceSnapshot(SdFat *sd,
                            const AssetData::RuntimeManifest &manifest,
                            RuntimeTableFile &tableFile)
{
    return tableFile.open(sd, manifest) &&
           appearanceReferencesAvailable(tableFile.table());
}
} // namespace

bool validateRuntimeTableAppearance(SdFat *sd,
                                    const AssetData::RuntimeManifest &manifest)
{
    RuntimeTableFile tableFile;
    return openAppearanceSnapshot(sd, manifest, tableFile) &&
           validateAppearanceSections(tableFile.table());
}

bool findRuntimeTableEvolutionTarget(SdFat *sd,
                                     const AssetData::RuntimeManifest &manifest,
                                     BundleReader &bundleReader,
                                     const ActivePetBehaviorStatSlots &activeSlots,
                                     const PetStatSnapshot &stats,
                                     AppearanceSelection &selection)
{
    selection = {};
    RuntimeTableFile tableFile;
    if (!openAppearanceSnapshot(sd, manifest, tableFile))
        return false;
    return (tableFile.table().featureFlags & (1UL << 3)) == 0 ||
           decodeEvolutionQuery(tableFile.table(), bundleReader, activeSlots, stats, selection);
}

bool loadRuntimeTableSpecies(SdFat *sd, const AssetData::RuntimeManifest &manifest,
                             uint8_t *species,
                             size_t maxSpecies, size_t &speciesCount)
{
    speciesCount = 0;
    if (species == nullptr || maxSpecies == 0)
        return false;
    RuntimeTableFile tableFile;
    return openAppearanceSnapshot(sd, manifest, tableFile) &&
           decodeSpeciesQuery(tableFile.table(), species, maxSpecies, speciesCount) &&
           speciesCount != 0;
}

bool loadRuntimeTableOutfits(SdFat *sd, const AssetData::RuntimeManifest &manifest,
                             uint8_t speciesSlot,
                             uint8_t unlockMask, uint8_t *outfits, size_t maxOutfits, size_t &outfitCount)
{
    outfitCount = 0;
    if (speciesSlot == 0 || outfits == nullptr || maxOutfits == 0)
        return false;
    RuntimeTableFile tableFile;
    return openAppearanceSnapshot(sd, manifest, tableFile) &&
           decodeOutfitList(tableFile.table(), speciesSlot, unlockMask,
                            outfits, maxOutfits, outfitCount) &&
           outfitCount != 0;
}

bool findRuntimeTableOutfitPreview(SdFat *sd, const AssetData::RuntimeManifest &manifest,
                                   BundleReader &bundleReader, uint8_t speciesSlot,
                                   uint8_t outfitSlot, bool locked, OutfitPreview &preview)
{
    preview = {};
    if (speciesSlot == 0 || outfitSlot == 0)
        return false;
    RuntimeTableFile tableFile;
    return openAppearanceSnapshot(sd, manifest, tableFile) &&
           decodeOutfitPreview(tableFile.table(), bundleReader,
                               speciesSlot, outfitSlot, locked, preview) &&
           preview.animation.valid();
}

bool resolveRuntimeTableOutfitUnlockMask(SdFat *sd,
                                         const AssetData::RuntimeManifest &manifest, uint8_t speciesSlot,
                                         const ActivePetBehaviorStatSlots &activeSlots,
                                         const PetStatSnapshot &stats, uint8_t currentMask, bool initialize,
                                         uint8_t &resolvedMask)
{
    resolvedMask = 0;
    if (speciesSlot == 0)
        return false;
    RuntimeTableFile tableFile;
    return openAppearanceSnapshot(sd, manifest, tableFile) &&
           decodeOutfitUnlocks(tableFile.table(), speciesSlot, activeSlots,
                               stats, currentMask, initialize, resolvedMask);
}

bool resolveRuntimeTableConsumableOutfitUnlock(
    SdFat *sd, const AssetData::RuntimeManifest &manifest,
    uint8_t speciesSlot, uint8_t outfitSlot,
    const ActivePetBehaviorStatSlots &activeSlots, const PetStatSnapshot &stats,
    PetStatSnapshot &consumedStats)
{
    consumedStats = {};
    RuntimeTableFile tableFile;
    return openAppearanceSnapshot(sd, manifest, tableFile) &&
           decodeConsumableOutfitUnlock(tableFile.table(), speciesSlot, outfitSlot,
                                        activeSlots, stats, consumedStats);
}
