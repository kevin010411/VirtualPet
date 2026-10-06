#include "appearance/detail/RuntimeTableAppearanceDecoder.h"
#include "resources/detail/RuntimeTableFile.h"
#include "pet/RuntimeValueResolver.h"

using namespace RuntimeTableInternal;

namespace
{
bool appearanceReferencesAvailable(const RuntimeTable &table)
{
    return (table.featureFlags & (1UL << 2)) != 0 &&
           table.find(AssetRefs) != nullptr && table.find(Animations) != nullptr;
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
    uint8_t sourcePlaybackCount = 0;
    uint8_t targetPlaybackCount = 0;
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
    decoded.sourcePlaybackCount = record[13];
    decoded.targetPlaybackCount = record[14];
    const bool playbackCountsValid =
        (decoded.sourceAnimationRef == kNone16 ? decoded.sourcePlaybackCount == 0
            : decoded.sourcePlaybackCount >= 1 && decoded.sourcePlaybackCount <= 5) &&
        (decoded.targetAnimationRef == kNone16 ? decoded.targetPlaybackCount == 0
            : decoded.targetPlaybackCount >= 1 && decoded.targetPlaybackCount <= 5);
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
           referencesMatchMode && playbackCountsValid && record[15] == 0;
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
        if (evolution.mode != EvolutionAnimationMode::Disabled &&
            (table.featureFlags & kEvolutionPlaybackCountsFeature) == 0)
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
            selection.sourceEvolutionPlaybackCount = evolution.sourcePlaybackCount;
            selection.targetEvolutionPlaybackCount = evolution.targetPlaybackCount;
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

} // namespace

namespace RuntimeTableInternal
{
bool decodeInitialAppearance(const RuntimeTable &table, BundleReader &bundleReader,
                             AppearanceSelection &selection)
{
    if (!appearanceReferencesAvailable(table))
        return false;
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
    return true;
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

} // namespace RuntimeTableInternal

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
