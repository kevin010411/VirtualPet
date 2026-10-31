#include <assert.h>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

#include "controller/StatusSetContract.h"
#include "controller/SystemCommandCatalog.h"
#include "appearance/RuntimeTableAppearance.h"
#include "appearance/SdAppearanceLoader.h"
#include "pet/PetBehaviorRuntimeRules.h"
#include "resources/RuntimeTableBehavior.h"
#include "resources/detail/RuntimeTableReader.h"

namespace AssetData
{
bool sameBundleId(const BundleId &left, const BundleId &right)
{
    for (uint8_t index = 0; index < sizeof(left.bytes); ++index)
        if (left.bytes[index] != right.bytes[index])
            return false;
    return true;
}

// Behavior fixtures do not mount asset packs. The complete SD-loader tests own
// physical pack resolution; this seam only makes the behavior reader linkable.
bool animationReferenceExists(BundleReader &, const AnimationRef &, uint8_t)
{
    return true;
}
} // namespace AssetData

BundleReader::BundleReader(SdFat *sd, uint8_t *scratch, size_t scratchSize)
    : sd_(sd), scratch_(scratch), scratchSize_(scratchSize)
{
}

bool BundleReader::configureBundle(const AssetData::BundleId &bundleId)
{
    bundleId_ = bundleId;
    bundleConfigured_ = true;
    return true;
}

const char *BundleReader::firstErrorResource() const
{
    return firstErrorResource_;
}

namespace
{
std::vector<uint8_t> readFixture(const char *path)
{
    std::ifstream input(path, std::ios::binary);
    assert(input);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(input),
                                std::istreambuf_iterator<char>());
}

uint32_t readFixtureU32(const std::vector<uint8_t> &fixture, size_t offset)
{
    assert(offset + 4 <= fixture.size());
    return static_cast<uint32_t>(fixture[offset]) |
           (static_cast<uint32_t>(fixture[offset + 1]) << 8U) |
           (static_cast<uint32_t>(fixture[offset + 2]) << 16U) |
           (static_cast<uint32_t>(fixture[offset + 3]) << 24U);
}

AssetData::RuntimeManifest fixtureManifest(const std::vector<uint8_t> &fixture)
{
    AssetData::RuntimeManifest manifest = {};
    const uint8_t bundleId[16] = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    for (uint8_t index = 0; index < sizeof(bundleId); ++index)
        manifest.bundleId.bytes[index] = bundleId[index];
    manifest.fileSize = static_cast<uint32_t>(fixture.size());
    manifest.schemaFingerprint = readFixtureU32(fixture, 44);
    manifest.fileCrc32 = readFixtureU32(fixture, 48);
    return manifest;
}

AssetData::RuntimeManifest releaseFixtureManifest(const std::vector<uint8_t> &fixture)
{
    AssetData::RuntimeManifest manifest = fixtureManifest(fixture);
    manifest.bundleId.bytes[6] = 0x46;
    return manifest;
}

uint16_t selectFirstOutcome(uint16_t)
{
    return 0;
}

// Replace only Action sections in a real exported v9 bundle. The physical
// asset/layout records remain the exporter's; these synthetic records exercise
// all Action modes without introducing an older-format compatibility reader.
void putFixtureU16(std::vector<uint8_t> &bytes, size_t offset, uint16_t value)
{
    bytes[offset] = static_cast<uint8_t>(value);
    bytes[offset + 1] = static_cast<uint8_t>(value >> 8);
}

void putFixtureU32(std::vector<uint8_t> &bytes, size_t offset, uint32_t value)
{
    putFixtureU16(bytes, offset, static_cast<uint16_t>(value));
    putFixtureU16(bytes, offset + 2, static_cast<uint16_t>(value >> 16));
}

struct FixtureSection
{
    uint16_t type;
    uint16_t recordSize;
    std::vector<uint8_t> records;
};

std::vector<uint8_t> actionModesFixture(const std::vector<uint8_t> &base)
{
    std::vector<FixtureSection> sections;
    uint16_t animationRef = 0;
    const uint16_t count = base[24] | (static_cast<uint16_t>(base[25]) << 8);
    for (uint16_t index = 0; index < count; ++index)
    {
        const size_t entry = 64 + index * 16;
        const uint16_t type = base[entry] | (static_cast<uint16_t>(base[entry + 1]) << 8);
        const uint16_t size = base[entry + 10] | (static_cast<uint16_t>(base[entry + 11]) << 8);
        const size_t offset = readFixtureU32(base, entry + 4);
        const size_t length = readFixtureU32(base, entry + 12);
        if (type == 13)
            animationRef = base[offset + 2] | (static_cast<uint16_t>(base[offset + 3]) << 8);
        if (type < 12 || type > 15)
            sections.push_back({type, size, {base.begin() + offset, base.begin() + offset + length}});
    }
    std::vector<uint8_t> actions(3 * 16, 0);
    const uint16_t firstOutcomes[] = {0, 1, 2};
    for (uint8_t index = 0; index < 3; ++index)
    {
        actions[index * 16] = index;
        actions[index * 16 + 1] = index; // standard, conditional, random
        actions[index * 16 + 2] = 3;
        actions[index * 16 + 3] = index == 1 ? 1 : 0;
        putFixtureU16(actions, index * 16 + 4, firstOutcomes[index]);
        actions[index * 16 + 6] = index == 2 ? 2 : 1;
        putFixtureU16(actions, index * 16 + 8, index == 2 ? 1 : 0);
        actions[index * 16 + 10] = index == 1 ? 1 : 0;
    }
    std::vector<uint8_t> outcomes(4 * 10, 0);
    std::vector<uint8_t> effects(4 * 12, 0);
    for (uint8_t index = 0; index < 4; ++index)
    {
        outcomes[index * 10] = index >= 2 ? 1 : 0;
        outcomes[index * 10 + 1] = 1;
        putFixtureU16(outcomes, index * 10 + 2, animationRef);
        putFixtureU16(outcomes, index * 10 + 4, index);
        putFixtureU16(outcomes, index * 10 + 6, 1);
        effects[index * 12 + 1] = 1; // set stat zero
        putFixtureU16(effects, index * 12 + 2, index + 1);
    }
    std::vector<uint8_t> conditions(24, 0);
    putFixtureU16(conditions, 2, runtimeValueIdForPetStat(0));
    conditions[4] = 5;
    putFixtureU16(conditions, 6, animationRef);
    putFixtureU32(conditions, 8, 10);
    putFixtureU32(conditions, 16, 10);
    sections.push_back({12, 16, actions});
    sections.push_back({13, 10, outcomes});
    sections.push_back({14, 24, conditions});
    sections.push_back({15, 12, effects});
    std::sort(sections.begin(), sections.end(), [](const FixtureSection &a, const FixtureSection &b) {
        return a.type < b.type;
    });
    std::vector<uint8_t> fixture(base.begin(), base.begin() + 64);
    fixture.resize(64 + sections.size() * 16);
    putFixtureU16(fixture, 24, static_cast<uint16_t>(sections.size()));
    for (size_t index = 0; index < sections.size(); ++index)
    {
        const auto &section = sections[index];
        fixture.resize((fixture.size() + 3) & ~size_t(3));
        const size_t entry = 64 + index * 16;
        putFixtureU16(fixture, entry, section.type);
        putFixtureU32(fixture, entry + 4, static_cast<uint32_t>(fixture.size()));
        putFixtureU16(fixture, entry + 8, static_cast<uint16_t>(section.records.size() / section.recordSize));
        putFixtureU16(fixture, entry + 10, section.recordSize);
        putFixtureU32(fixture, entry + 12, static_cast<uint32_t>(section.records.size()));
        fixture.insert(fixture.end(), section.records.begin(), section.records.end());
    }
    putFixtureU32(fixture, 16, static_cast<uint32_t>(fixture.size()));
    return fixture;
}

uint16_t selectLastOutcome(uint16_t upperExclusive) { return upperExclusive - 1; }

void testEvolutionPlaybackRecords(const std::vector<uint8_t> &exported)
{
    size_t evolutionOffset = 0;
    const uint16_t sectionCount = exported[24] | (static_cast<uint16_t>(exported[25]) << 8);
    for (uint16_t index = 0; index < sectionCount; ++index)
    {
        const size_t entry = 64 + index * 16;
        if (exported[entry] == 33 && exported[entry + 1] == 0)
            evolutionOffset = readFixtureU32(exported, entry + 4);
    }
    assert(evolutionOffset != 0);
    for (uint8_t variant = 0; variant < 5; ++variant)
    {
        auto fixture = exported;
        putFixtureU32(fixture, 12, readFixtureU32(fixture, 12) | (1UL << 12));
        putFixtureU16(fixture, evolutionOffset + 8, 0);
        putFixtureU16(fixture, evolutionOffset + 10, 0);
        fixture[evolutionOffset + 12] = 2;
        fixture[evolutionOffset + 13] = variant == 1 ? 1 : 3;
        fixture[evolutionOffset + 14] = 5;
        if (variant == 2) fixture[evolutionOffset + 13] = 0;
        if (variant == 3) fixture[evolutionOffset + 14] = 6;
        if (variant == 4)
            putFixtureU32(fixture, 12, readFixtureU32(fixture, 12) & ~(1UL << 12));
        SdFat sd(fixture.data(), fixture.size());
        uint8_t scratch[AssetData::kIoScratchBytes] = {};
        BundleReader reader(&sd, scratch, sizeof(scratch));
        PetBehaviorConfig config = {};
        const auto manifest = releaseFixtureManifest(fixture);
        assert(parseRuntimeTableBehavior(fixture.data(), fixture.size(), manifest, 1, 1, config));
        ActivePetBehaviorStatSlots activeSlots(config);
        PetStatSnapshot stats = {};
        stats.speciesSlot = 1;
        stats.outfitSlot = 1;
        stats.stage_days = 100;
        AppearanceSelection selection = {};
        const bool accepted = findRuntimeTableEvolutionTarget(
            &sd, manifest, reader, activeSlots, stats, selection);
        // Playback-count semantics are exporter-owned; the missing execution
        // feature remains a firmware compatibility failure.
        assert(accepted == (variant != 4));
        if (accepted)
        {
            assert(selection.speciesSlot == 2 && selection.outfitSlot == 1);
            assert(selection.sourceEvolutionPlaybackCount == fixture[evolutionOffset + 13]);
            assert(selection.targetEvolutionPlaybackCount == fixture[evolutionOffset + 14]);
        }
    }
}

void testEvolutionConditionRanges(const std::vector<uint8_t> &exported)
{
    size_t evolutionOffset = 0;
    size_t conditionEntry = 0;
    const uint16_t sectionCount = exported[24] | (static_cast<uint16_t>(exported[25]) << 8);
    for (uint16_t index = 0; index < sectionCount; ++index)
    {
        const size_t entry = 64 + index * 16;
        if (exported[entry] == 33 && exported[entry + 1] == 0)
            evolutionOffset = readFixtureU32(exported, entry + 4);
        if (exported[entry] == 34 && exported[entry + 1] == 0)
            conditionEntry = entry;
    }
    assert(evolutionOffset != 0 && conditionEntry != 0);
    for (uint8_t variant = 0; variant < 5; ++variant)
    {
        auto fixture = exported;
        const uint8_t conditionCount = variant == 0 ? 5 : variant == 4 ? 255 : 6;
        const uint16_t storedCount = variant == 3 ? conditionCount - 1 : conditionCount;
        fixture.resize((fixture.size() + 3) & ~size_t(3));
        const size_t conditionOffset = fixture.size();
        fixture.resize(conditionOffset + storedCount * 20, 0);
        for (uint16_t index = 0; index < storedCount; ++index)
        {
            const size_t offset = conditionOffset + index * 20;
            // stage_days AND all five Pet Stats, as in project_26. Extra wire
            // predicates exercise the uint8 count boundary without new storage.
            const RuntimeValueId valueId = index == 0 || index > 5
                ? kRuntimeValueStageDays : runtimeValueIdForPetStat(index - 1);
            putFixtureU16(fixture, offset, valueId);
            putFixtureU32(fixture, offset + 2, variant == 2 && index == 5 ? 1 : 0);
            putFixtureU32(fixture, offset + 10, 864000);
        }
        putFixtureU32(fixture, conditionEntry + 4, static_cast<uint32_t>(conditionOffset));
        putFixtureU16(fixture, conditionEntry + 8, storedCount);
        putFixtureU32(fixture, conditionEntry + 12, storedCount * 20);
        fixture[evolutionOffset + 5] = conditionCount;
        putFixtureU16(fixture, evolutionOffset + 6, 0);
        putFixtureU32(fixture, 16, static_cast<uint32_t>(fixture.size()));
        SdFat sd(fixture.data(), fixture.size());
        PetBehaviorConfig config = {};
        const auto manifest = releaseFixtureManifest(fixture);
        assert(parseRuntimeTableBehavior(fixture.data(), fixture.size(), manifest, 1, 1, config));
        SdAppearanceLoader loader(&sd);
        loader.configureRuntimeContract(config);
        PetStatSnapshot stats = {};
        stats.speciesSlot = 1;
        stats.outfitSlot = 1;
        AppearanceSelection selection = {};
        const auto result = loader.findEvolutionTarget(stats, selection);
        assert(result == (variant == 3 ? EvolutionLookupResult::LoadFailed
            : variant == 2 ? EvolutionLookupResult::NoTarget : EvolutionLookupResult::Found));
        if (result == EvolutionLookupResult::Found)
            assert(selection.speciesSlot == 2 && selection.outfitSlot == 1);
        if (result == EvolutionLookupResult::LoadFailed)
            assert(std::strcmp(loader.firstAssetDataErrorResource(), "runtime") == 0);
    }
}

void testSdCatchUpThresholds(const std::vector<uint8_t> &exported)
{
    // Neutralize unrelated thresholds, then exercise the production SD query
    // with one-day windows in both Evolution and Outfit unlock sections.
    auto fixture = exported;
    std::vector<size_t> thresholds;
    const uint16_t sectionCount = fixture[24] | (uint16_t(fixture[25]) << 8);
    for (uint16_t index = 0; index < sectionCount; ++index)
    {
        const size_t entry = 64 + index * 16;
        if (fixture[entry] != 34 && fixture[entry] != 36) continue;
        const uint16_t count = fixture[entry + 8] | (uint16_t(fixture[entry + 9]) << 8);
        const size_t offset = readFixtureU32(fixture, entry + 4);
        if (count) thresholds.push_back(offset);
        for (uint16_t row = 0; row < count; ++row)
        {
            putFixtureU16(fixture, offset + row * 20, runtimeValueIdForPetStat(1));
            putFixtureU32(fixture, offset + row * 20 + 2, 0);
            putFixtureU32(fixture, offset + row * 20 + 10, 100);
        }
    }
    assert(thresholds.size() == 2);
    for (size_t offset : thresholds)
    {
        auto selected = fixture;
        putFixtureU16(selected, offset, runtimeValueIdForPetStat(0));
        putFixtureU32(selected, offset + 2, 95);
        putFixtureU32(selected, offset + 10, 95);
        SdFat sd(selected.data(), selected.size());
        PetBehaviorConfig config = {};
        config.assetManifest = releaseFixtureManifest(selected);
        config.stats[0] = {true, 100, 0, 100, -1};
        config.stats[1] = {true, 0, 0, 100, 0};
        SdAppearanceLoader loader(&sd);
        loader.configureRuntimeContract(config);
        PetBehaviorStatValues state = {};
        state.values[0] = 100;
        PetBehaviorDailyChangePauses pauses = {};
        uint32_t days = 0;
        assert(loader.catchUpDays(config, state, pauses, 100, days) && days == 5);
        state.values[0] = 95;
        assert(loader.catchUpDays(config, state, pauses, 100, days) && days == 1);
        state.values[0] = 94;
        assert(loader.catchUpDays(config, state, pauses, 100, days) && days == 100);
        HostSd::mountedData = nullptr;
        assert(!loader.catchUpDays(config, state, pauses, 100, days));
        assert(std::strcmp(loader.firstAssetDataErrorResource(), "runtime") == 0);
    }
}

void testDecodedActionRanges(const std::vector<uint8_t> &exported)
{
    auto fixture = actionModesFixture(exported);
    PetBehaviorConfig config = {};
    assert(parseRuntimeTableBehavior(fixture.data(), fixture.size(),
                                     releaseFixtureManifest(fixture), 1, 1, config));
    assert(config.actionCount == 3 && config.actionOutcomeCount == 4);
    assert(config.actionEffectCount == 4 && config.actionConditionCount == 1);
    assert(config.actions[2].firstOutcome == 2 && config.actions[2].outcomeCount == 2);
    PetBehaviorStatValues state = {};
    PetBehaviorDailyChangePauses pauses = {};
    PetBehaviorActionPlayback playback = {};
    assert(config.stats[0].minValue <= 1 && config.stats[0].maxValue >= 10);
    state.values[0] = 10;
    assert(applyPetBehaviorAction(config, 0, state, pauses, playback));
    assert(state.values[0] == 1 && playback.playbackCount == 1);
    state.values[0] = 10;
    assert(applyPetBehaviorAction(config, 1, state, pauses, playback));
    assert(state.values[0] == 2 && playback.playbackCount == 5);
    assert(applyPetBehaviorAction(config, 1, state, pauses, playback));
    assert(state.values[0] == 2 && playback.playbackCount == 1);
    assert(applyPetBehaviorAction(config, 2, state, pauses, playback, selectFirstOutcome));
    assert(state.values[0] == 3);
    assert(applyPetBehaviorAction(config, 2, state, pauses, playback, selectLastOutcome));
    assert(state.values[0] == 4);

    // Exporter validates child ownership. The firmware decodes canonical
    // contiguous effects without repeating that semantic comparison.
    for (size_t entry = 64; entry < 64U + fixture[24] * 16U; entry += 16)
    {
        if (fixture[entry] != 13) continue;
        const size_t offset = readFixtureU32(fixture, entry + 4);
        putFixtureU16(fixture, offset + 10 + 4, 0);
        break;
    }
    assert(parseRuntimeTableBehavior(fixture.data(), fixture.size(),
                                     releaseFixtureManifest(fixture), 1, 1, config));
    state.values[0] = 10;
    assert(applyPetBehaviorAction(config, 1, state, pauses, playback));
    assert(state.values[0] == 2);
}
struct StatusContext
{
    const PetBehaviorStatValues *stats;
    uint32_t stageDays;
};

bool statusValue(const StatusSetCondition &condition,
                 const void *rawContext,
                 int64_t &value)
{
    const StatusContext &context = *static_cast<const StatusContext *>(rawContext);
    if (condition.kind != StatusConditionKind::RuntimeValue)
        return false;
    if (condition.valueId == kRuntimeValueStageDays)
    {
        value = static_cast<int32_t>(context.stageDays);
        return true;
    }
    if (!isRuntimeValueIdPetStat(condition.valueId) ||
        runtimePetStatSlot(condition.valueId) >= 10)
        return false;
    value = context.stats->values[runtimePetStatSlot(condition.valueId)];
    return true;
}

void testBehaviorFullFixture(const std::vector<uint8_t> &fixture)
{
    PetBehaviorConfig config = {};
    const AssetData::RuntimeManifest manifest = fixtureManifest(fixture);
    assert(parseRuntimeTableBehavior(fixture.data(), fixture.size(), manifest, 1, 1, config));
    assert(config.schemaFingerprint == 0x12345678UL);
    assert(config.statCount == 10);
    assert(config.actionCount == 8);
    assert(config.actionConditionCount == 8);
    assert(config.statusSets.count == 2);
    assert(config.statusSets.sets[0].conditions[0].valueId == runtimeValueIdForPetStat(0));
    assert(config.statusSets.sets[0].conditions[1].valueId == runtimeValueIdForPetStat(1));
    assert(config.statusSets.sets[0].conditions[2].valueId == runtimeValueIdForPetStat(2));
    assert(config.statusSets.sets[0].conditions[0].petStateMask == 0);
    assert(config.buttons[0].kind == PetBehaviorButtonKind::UserAction);
    assert(config.buttons[0].actionSlot == 0);

    PetBehaviorStatValues state = {};
    PetBehaviorDailyChangePauses pauses = {};
    initializePetBehaviorStats(config, state);
    PetBehaviorActionPlayback playback = {};

    // Standard mode commits every effect before the caller attempts playback.
    assert(applyPetBehaviorAction(config, 0, state, pauses, playback, selectFirstOutcome));
    assert(playback.animation.animationId == 1);
    assert(playback.playbackCount == 5);
    assert(state.values[0] == 51);
    assert(state.values[9] == 60);
    assert(pauses.remainingDays[0] == 255);
    applyPetBehaviorDailyChanges(config, state, pauses);
    assert(state.values[0] == 51);
    assert(pauses.remainingDays[0] == 254);

    // Dropping an otherwise valid playback request models a missing or full
    // animation queue at the rules seam; committed values are not rolled back.
    playback = {};
    assert(state.values[0] == 51);
    assert(pauses.remainingDays[0] == 254);

    // Conditional mode chooses from pre-effect values, then commits its
    // implicit Outcome. With all Stats at their initial 50, priority 3 wins.
    state = {};
    pauses = {};
    initializePetBehaviorStats(config, state);
    assert(applyPetBehaviorAction(config, 1, state, pauses, playback, selectFirstOutcome));
    assert(playback.animation.animationId == 4);
    assert(playback.playbackCount == 1);
    assert(state.values[0] == 51);

    // Random mode uses the bounded source to select the first weighted Outcome.
    state = {};
    pauses = {};
    initializePetBehaviorStats(config, state);
    assert(applyPetBehaviorAction(config, 3, state, pauses, playback, selectFirstOutcome));
    assert(playback.animation.animationId == 10);
    assert(playback.playbackCount == 5);
    assert(state.values[9] == 60);

    StatusSetResolution resolution = {};
    const StatusContext context = {&state, 100};
    assert(resolveStatusSet(config.statusSets.sets[0], statusValue, &context, resolution));
    assert(resolution.playOnce);
    assert(resolution.animation.animationId == 31);
    assert(resolveStatusSet(config.statusSets.sets[1], statusValue, &context, resolution));
    assert(resolution.playOnce);
    assert(resolution.requiredVersions == 24);
    assert(resolution.versionIndex < resolution.requiredVersions);
    assert(resolution.animation.animationId == 32);
}

void testInvalidFixtureFailsWithoutPartialPublication(const std::vector<uint8_t> &fixture)
{
    PetBehaviorConfig config = {};
    config.schemaFingerprint = 0xa5a5a5a5UL;
    config.statCount = 7;
    const AssetData::RuntimeManifest manifest = fixtureManifest(fixture);
    assert(!parseRuntimeTableBehavior(fixture.data(), fixture.size(), manifest, 1, 1, config));
    assert(config.schemaFingerprint == 0xa5a5a5a5UL);
    assert(config.statCount == 7);
}

void testValidFixtureLoads(const std::vector<uint8_t> &fixture, const char *label)
{
    PetBehaviorConfig config = {};
    const bool accepted = parseRuntimeTableBehavior(
        fixture.data(), fixture.size(), fixtureManifest(fixture), 1, 1, config);
    if (!accepted)
        printf("valid fixture rejected: %s\n", label);
    assert(accepted);
}

void testOutfitSelectionReleaseFixture(const std::vector<uint8_t> &fixture)
{
    assert(findCompiledSystemCommand(RuntimeSystemCommandId::ChangeSpecies) == nullptr);
    SdFat sd(fixture.data(), fixture.size());
    uint8_t scratch[AssetData::kIoScratchBytes] = {};
    BundleReader reader(&sd, scratch, sizeof(scratch));
    const AssetData::RuntimeManifest manifest = releaseFixtureManifest(fixture);

    PetBehaviorConfig config = {};
    assert(parseRuntimeTableBehavior(fixture.data(), fixture.size(), manifest, 1, 1, config));

    ActivePetBehaviorStatSlots activeSlots(config);
    PetStatSnapshot stats = {};
    stats.speciesSlot = 1;
    stats.outfitSlot = 7;
    stats.customStats[0] = 50;

    uint8_t unlockMask = 0;
    assert(resolveRuntimeTableOutfitUnlockMask(
        &sd, manifest, 1, activeSlots, stats, 0, true, unlockMask));
    assert(unlockMask == 0xE3U);

    uint8_t outfits[8] = {};
    size_t outfitCount = 0;
    assert(loadRuntimeTableOutfits(
        &sd, manifest, 1, unlockMask, outfits, 8, outfitCount));
    const uint8_t expectedVisible[] = {1, 2, 4, 5, 6, 7, 8};
    assert(outfitCount == sizeof(expectedVisible));
    for (size_t index = 0; index < outfitCount; ++index)
        assert(outfits[index] == expectedVisible[index]);

    // Species 2 starts after Species 1's eight records in the exported table.
    outfitCount = 0;
    assert(loadRuntimeTableOutfits(
        &sd, manifest, 2, 0x01U, outfits, 8, outfitCount));
    assert(outfitCount == 1 && outfits[0] == 1);
    assert(!loadRuntimeTableOutfits(
        &sd, manifest, 3, 0x01U, outfits, 8, outfitCount));

    OutfitPreview locked = {};
    assert(findRuntimeTableOutfitPreview(&sd, manifest, reader, 1, 4, true, locked));
    assert(locked.speciesSlot == 1 && locked.outfitSlot == 4 && locked.animation.valid());
    OutfitPreview secondSpecies = {};
    assert(findRuntimeTableOutfitPreview(&sd, manifest, reader, 2, 1, false, secondSpecies));
    assert(secondSpecies.speciesSlot == 2 && secondSpecies.outfitSlot == 1);
    assert(!findRuntimeTableOutfitPreview(&sd, manifest, reader, 2, 2, false, secondSpecies));

    stats.stage_days = 10;
    assert(resolveRuntimeTableOutfitUnlockMask(
        &sd, manifest, 1, activeSlots, stats, unlockMask, false, unlockMask));
    assert(unlockMask == 0xFBU);
    stats.stage_days = 0;
    stats.customStats[0] = 0;
    assert(resolveRuntimeTableOutfitUnlockMask(
        &sd, manifest, 1, activeSlots, stats, unlockMask, false, unlockMask));
    assert(unlockMask == 0xFBU);

    uint8_t resetMask = 0;
    assert(resolveRuntimeTableOutfitUnlockMask(
        &sd, manifest, 1, activeSlots, stats, 0, true, resetMask));
    assert(resetMask == 0xC3U);

    std::vector<uint8_t> badRange = fixture;
    const uint16_t sectionCount = static_cast<uint16_t>(
        badRange[24] | (static_cast<uint16_t>(badRange[25]) << 8U));
    bool changed = false;
    for (uint16_t section = 0; section < sectionCount; ++section)
    {
        const size_t directoryOffset = 64U + static_cast<size_t>(section) * 16U;
        if (badRange[directoryOffset] != 31 || badRange[directoryOffset + 1] != 0)
            continue;
        const size_t speciesOffset = readFixtureU32(badRange, directoryOffset + 4U);
        badRange[speciesOffset + 8U + 2U] = 9; // Species 2 range now exceeds Outfits.
        badRange[speciesOffset + 8U + 3U] = 0;
        changed = true;
        break;
    }
    assert(changed);
    SdFat badSd(badRange.data(), badRange.size());
    outfitCount = 0;
    assert(!loadRuntimeTableOutfits(&badSd, releaseFixtureManifest(badRange),
                                    2, 0x01U, outfits, 8, outfitCount));
    assert(outfitCount == 0);

    std::vector<uint8_t> consumable = fixture;
    bool changedUnlock = false;
    for (uint16_t section = 0; section < sectionCount; ++section)
    {
        const size_t directoryOffset = 64U + static_cast<size_t>(section) * 16U;
        if (consumable[directoryOffset] != 35 || consumable[directoryOffset + 1] != 0)
            continue;
        const size_t unlockOffset = readFixtureU32(consumable, directoryOffset + 4U);
        consumable[unlockOffset + 3U * 8U + 2U] = 2; // Outfit 4 consumes stage days.
        changedUnlock = true;
        break;
    }
    assert(changedUnlock);
    SdFat consumeSd(consumable.data(), consumable.size());
    PetStatSnapshot before = {};
    before.speciesSlot = 1;
    before.outfitSlot = 1;
    before.stage_days = 10;
    PetStatSnapshot after = {};
    assert(resolveRuntimeTableConsumableOutfitUnlock(
        &consumeSd, releaseFixtureManifest(consumable),
        1, 4, activeSlots, before, after));
    assert(after.stage_days == 2 && after.speciesSlot == 1);
}

void testAppearanceQueryAdapter(const std::vector<uint8_t> &fixture)
{
    SdFat sd(fixture.data(), fixture.size());
    PetBehaviorConfig config = {};
    const AssetData::RuntimeManifest manifest = releaseFixtureManifest(fixture);
    assert(parseRuntimeTableBehavior(fixture.data(), fixture.size(), manifest, 1, 1, config));
    SdAppearanceLoader loader(&sd);
    loader.configureRuntimeContract(config);

    uint8_t outfits[8] = {};
    size_t outfitCount = 0;
    HostSd::openCount = 0;
    assert(loader.loadOutfits(1, 0xE3U, outfits, 8, outfitCount));
    assert(outfitCount == 7 && HostSd::openCount == 1);

    OutfitPreview preview = {};
    HostSd::openCount = 0;
    assert(loader.findOutfitPreview(1, 4, true, preview));
    assert(preview.animation.valid() && HostSd::openCount == 1);

    PetStatSnapshot stats = {};
    stats.speciesSlot = 1;
    stats.outfitSlot = 7;
    stats.customStats[0] = 50;
    uint8_t unlockMask = 0;
    HostSd::openCount = 0;
    assert(loader.resolveOutfitUnlockMask(1, stats, 0, true, unlockMask));
    assert(unlockMask == 0xE3U && HostSd::openCount == 1);
    assert(loader.firstAssetDataErrorResource()[0] == '\0');

    PetBehaviorConfig mismatched = config;
    ++mismatched.assetManifest.fileSize;
    SdAppearanceLoader invalidLoader(&sd);
    invalidLoader.configureRuntimeContract(mismatched);
    HostSd::openCount = 0;
    outfitCount = 0;
    assert(!invalidLoader.loadOutfits(1, 0xE3U, outfits, 8, outfitCount));
    assert(outfitCount == 0);
    assert(HostSd::openCount == 1);
    assert(strcmp(invalidLoader.firstAssetDataErrorResource(), "runtime") == 0);
}

void testInvalidAppearanceFixture(const std::vector<uint8_t> &fixture)
{
    SdFat sd(fixture.data(), fixture.size());
    PetBehaviorConfig published = {};
    published.schemaFingerprint = 0xA5A5A5A5UL;
    published.statCount = 7;
    PetBehaviorConfig candidate = published;
    const AssetData::RuntimeManifest manifest = releaseFixtureManifest(fixture);
    bool accepted = parseRuntimeTableBehavior(
        fixture.data(), fixture.size(), manifest, 1, 1, candidate);
    if (accepted)
        accepted = validateRuntimeTableAppearance(&sd, manifest);
    if (accepted)
        published = candidate;
    assert(!accepted);
    assert(published.schemaFingerprint == 0xA5A5A5A5UL);
    assert(published.statCount == 7);
}
} // namespace

// Exercise the public loaders so ownership stays covered across source splits.
void testRuntimeSnapshotLifecycle(const std::vector<uint8_t> &fixture)
{
    SdFat sd(fixture.data(), fixture.size());
    AssetData::RuntimeManifest manifest = {};
    HostSd::openCount = HostSd::closeCount = 0;
    assert(loadRuntimeManifest(&sd, manifest));
    assert(manifest.fileSize == fixture.size());
    assert(manifest.schemaFingerprint == readFixtureU32(fixture, 44));
    assert(manifest.fileCrc32 == readFixtureU32(fixture, 48));
    assert(HostSd::openCount == 1 && HostSd::closeCount == 1);

    HostSd::openCount = HostSd::closeCount = 0;
    assert(validateRuntimeTableAppearance(&sd, manifest));
    assert(HostSd::openCount == 1 && HostSd::closeCount == 1);

    auto mismatched = manifest;
    ++mismatched.schemaFingerprint;
    HostSd::openCount = HostSd::closeCount = 0;
    assert(!validateRuntimeTableAppearance(&sd, mismatched));
    assert(HostSd::openCount == 1 && HostSd::closeCount == 1);

    // Repeated queries must read the current snapshot, even after a valid load.
    for (size_t length : {size_t(0), size_t(63), fixture.size() - 1})
    {
        HostSd::mountedSize = length;
        HostSd::openCount = HostSd::closeCount = 0;
        auto rejected = manifest;
        assert(!loadRuntimeManifest(&sd, rejected));
        assert(rejected.fileSize == 0 && rejected.schemaFingerprint == 0);
        assert(HostSd::openCount == 1 && HostSd::closeCount == 1);
        assert(!validateRuntimeTableAppearance(&sd, manifest));
        assert(HostSd::openCount == 2 && HostSd::closeCount == 2);
    }
    HostSd::mountedSize = fixture.size();
    assert(validateRuntimeTableAppearance(&sd, manifest));

    HostSd::mountedData = nullptr;
    HostSd::openCount = HostSd::closeCount = 0;
    assert(!loadRuntimeManifest(&sd, manifest));
    assert(manifest.fileSize == 0);
    assert(HostSd::openCount == 1 && HostSd::closeCount == 0);
    assert(!loadRuntimeManifest(nullptr, manifest));
    assert(HostSd::openCount == 1);
    HostSd::mountedData = fixture.data();
}

void testDeathCapability(const std::vector<uint8_t> &base)
{
    auto fixture = base;
    size_t deathOffset = 0;
    uint16_t animationReference = 0;
    const uint16_t count = fixture[24] | (static_cast<uint16_t>(fixture[25]) << 8);
    for (uint16_t index = 0; index < count; ++index)
    {
        const size_t entry = 64 + index * 16;
        const uint16_t type = fixture[entry] | (static_cast<uint16_t>(fixture[entry + 1]) << 8);
        const size_t offset = readFixtureU32(fixture, entry + 4);
        if (type == 22) deathOffset = offset;
        if (type == 13)
            animationReference = fixture[offset + 2] | (static_cast<uint16_t>(fixture[offset + 3]) << 8);
    }
    assert(deathOffset != 0);
    fixture[deathOffset] = 1;
    putFixtureU16(fixture, deathOffset + 2, 1);
    putFixtureU16(fixture, deathOffset + 4, animationReference);
    PetBehaviorConfig config = {};
    assert(parseRuntimeTableBehavior(fixture.data(), fixture.size(),
                                    releaseFixtureManifest(fixture), 1, 1, config) == bool(ENABLE_DEATH));
    fixture[deathOffset] = 0;
    assert(parseRuntimeTableBehavior(fixture.data(), fixture.size(),
                                    releaseFixtureManifest(fixture), 1, 1, config));
    assert(!config.deathEnabled);
    assert(config.deathAnimation.valid() == bool(ENABLE_DEATH));
}

int main(int argc, char **argv)
{
    AssetData::AssetFrameAddress ninthSpecies = {};
    ninthSpecies.speciesSlot = 9;
    ninthSpecies.outfitSlot = 1;
    ninthSpecies.animationId = 1;
    assert(AssetData::isValidFrameAddress(ninthSpecies));
    ninthSpecies.outfitSlot = 9;
    assert(!AssetData::isValidFrameAddress(ninthSpecies));
#if RUNTIME_TABLE_INDEPENDENT_LOTTERY
    assert(argc == 2);
    const auto bytes = readFixture(argv[1]);
    AssetData::RuntimeManifest manifest = fixtureManifest(bytes);
    memcpy(manifest.bundleId.bytes, bytes.data() + 28, 16);
    PetBehaviorConfig config = {};
    assert(parseRuntimeTableBehavior(bytes.data(), bytes.size(), manifest, 1, 1, config));
    assert(config.actionCount == 16 && config.actionOutcomeCount == 248);
    assert(config.buttons[1].actionSlot == 15);
    for (uint8_t slot = 0; slot < 16; ++slot)
    {
        PetBehaviorStatValues values = {};
        initializePetBehaviorStats(config, values);
        PetBehaviorDailyChangePauses pauses = {};
        PetBehaviorActionPlayback playback = {};
        assert(applyPetBehaviorAction(config, slot, values, pauses, playback, [](uint16_t) -> uint16_t { return 0; }));
        assert(config.actions[slot].outcomeCount == (slot < 8 ? 3 : 28));
        assert(playback.versionSelection == (slot < 8 ? 0 : 1));
    }
    puts("[PASS] Actual Web export: eight ordinary actions plus eight independent lotteries, 248 outcomes and button slot15");
#elif RUNTIME_TABLE_LOTTERY
    assert(argc == 2);
    const auto bytes = readFixture(argv[1]);
    AssetData::RuntimeManifest manifest = fixtureManifest(bytes);
    memcpy(manifest.bundleId.bytes, bytes.data() + 28, 16);
    PetBehaviorConfig config = {};
    assert(parseRuntimeTableBehavior(bytes.data(), bytes.size(), manifest, 1, 1, config));
    assert(config.actionCount == 8 && config.actionOutcomeCount == 224);
    assert(config.actionEffectCount == 2688 && config.actionEffectsStreamed);
    uint8_t exportedOutfitCount = 0;
    const uint16_t sectionCount = RuntimeTableInternal::readU16(bytes.data() + 24);
    for (uint16_t section = 0; section < sectionCount; ++section)
    {
        const uint8_t *entry = bytes.data() + 64 + section * 16;
        if (RuntimeTableInternal::readU16(entry) == RuntimeTableInternal::Outfits)
            exportedOutfitCount = RuntimeTableInternal::readU16(entry + 8);
    }
    assert(exportedOutfitCount >= 1 && exportedOutfitCount <= 8);
    for (uint8_t outfit = 2; outfit <= exportedOutfitCount; ++outfit)
    {
        PetBehaviorConfig alternate = {};
        assert(parseRuntimeTableBehavior(bytes.data(), bytes.size(), manifest, 1, outfit, alternate));
        for (uint16_t index = 0; index < 224; ++index)
        {
            const auto &primary = alternate.actionOutcomes[index].animationPlayback;
            assert(primary.versionSelection == config.actionOutcomes[index].animationPlayback.versionSelection);
            assert(primary.animation.animationId == config.actionOutcomes[index].animationPlayback.animation.animationId);
            assert(primary.animation.outfitSlot == outfit);
        }
    }
    printf("[PASS] Maximum lottery mappings resolved for %u exported Outfits\n", exportedOutfitCount);
    struct Effects { const std::vector<uint8_t> &bytes; uint32_t offset; unsigned reads; } effects = {bytes, config.actionEffectsOffset, 0};
    const auto reader = [](void *context, uint16_t row, PetBehaviorActionEffectConfig &effect) -> bool {
        auto &source = *static_cast<Effects *>(context);
        const auto *record = source.bytes.data() + source.offset + row * 12;
        ++source.reads;
        effect = {record[0], static_cast<PetBehaviorEffectOperation>(record[1]), RuntimeTableInternal::readI64(record + 2)};
        return true;
    };
    static uint16_t draw;
    const auto random = [](uint16_t) -> uint16_t { return draw; };
    for (uint8_t actionSlot = 0; actionSlot < 8; ++actionSlot)
    {
        const auto &action = config.actions[actionSlot];
        assert(action.outcomeCount == 28);
        uint16_t weight = 0;
        for (uint8_t option = 0; option < 28; ++option)
        {
            const auto &outcome = config.actionOutcomes[action.firstOutcome + option];
            PetBehaviorStatValues values = {};
            initializePetBehaviorStats(config, values);
            auto expected = values;
            for (uint8_t effect = 0; effect < 12; ++effect)
            {
                const auto *record = bytes.data() + config.actionEffectsOffset + (outcome.firstEffect + effect) * 12;
                const uint8_t stat = record[0];
                const auto delta = RuntimeTableInternal::readI64(record + 2);
                const int64_t value = (record[1] == 1 ? 0 : expected.values[stat]) + delta;
                expected.values[stat] = value < 0 ? 0 : value > config.stats[stat].maxValue ? config.stats[stat].maxValue : value;
            }
            PetBehaviorDailyChangePauses pauses = {};
            PetBehaviorActionPlayback playback = {};
            effects.reads = 0;
            draw = weight;
            assert(applyPetBehaviorAction(config, actionSlot, values, pauses, playback, random, reader, &effects));
            assert(effects.reads == 12 && playback.versionSelection == option + 1);
            assert(memcmp(values.values, expected.values, sizeof(values.values)) == 0);
            weight += outcome.weight;
        }
    }
#elif RUNTIME_TABLE_V8
    assert(argc >= 5);
    const std::vector<uint8_t> valid = readFixture(argv[1]);
    testDeathCapability(valid);
    testDecodedActionRanges(valid);
    const std::vector<uint8_t> legacy = readFixture(argv[2]);
    const std::vector<uint8_t> startup = readFixture(argv[3]);
    testRuntimeSnapshotLifecycle(startup);
    testEvolutionPlaybackRecords(startup);
    testEvolutionConditionRanges(startup);
    testSdCatchUpThresholds(startup);
    PetBehaviorConfig config = {};
    assert(parseRuntimeTableBehavior(valid.data(), valid.size(),
                                     releaseFixtureManifest(valid), 1, 1, config));
    assert(config.layoutUnselected.valid() && config.layoutUnselected.shared());
    assert(config.layoutSelected.valid() && config.layoutSelected.shared());
    SdFat sd(startup.data(), startup.size());
    uint8_t scratch[AssetData::kIoScratchBytes] = {};
    BundleReader reader(&sd, scratch, sizeof(scratch));
    AppearanceSelection initial = {};
    bool initialResolved = false;
    HostSd::openCount = 0;
    const bool initialLoaded = loadCompleteRuntimeTable(
        &sd, releaseFixtureManifest(startup), reader,
        0, 0, config, &initial, &initialResolved);
    if (!initialLoaded)
        printf("initial contract failed: resolved=%d species=%u outfit=%u opens=%zu\n",
               initialResolved, initial.speciesSlot, initial.outfitSlot, HostSd::openCount);
    assert(initialLoaded);
    assert(initialResolved && initial.speciesSlot == 1 && initial.outfitSlot == 1);
    assert(config.activeSpeciesSlot == initial.speciesSlot &&
           config.activeOutfitSlot == initial.outfitSlot);
    assert(HostSd::openCount == 1);
    testAppearanceQueryAdapter(startup);
    HostSd::openCount = 0;
    assert(loadCompleteRuntimeTable(&sd, releaseFixtureManifest(startup), reader,
                                    1, 1, config));
    assert(HostSd::openCount == 1);

    // Replace OutfitUnlocks with a duplicate same-sized section. The initial
    // appearance and behavior remain decodable, but the required appearance
    // section check must reject the contract in the same read.
    std::vector<uint8_t> missingUnlocks = startup;
    const uint16_t sectionCount = static_cast<uint16_t>(
        startup[24] | (static_cast<uint16_t>(startup[25]) << 8U));
    bool replacedUnlocks = false;
    for (uint16_t index = 0; index < sectionCount; ++index)
    {
        const size_t offset = 64U + static_cast<size_t>(index) * 16U;
        if (missingUnlocks[offset] == 35 && missingUnlocks[offset + 1] == 0)
        {
            missingUnlocks[offset] = 32; // Outfits has the same record size.
            replacedUnlocks = true;
            break;
        }
    }
    assert(replacedUnlocks);
    SdFat missingUnlocksSd(missingUnlocks.data(), missingUnlocks.size());
    BundleReader missingUnlocksReader(&missingUnlocksSd, scratch, sizeof(scratch));
    assert(!validateRuntimeTableAppearance(&missingUnlocksSd,
                                           releaseFixtureManifest(missingUnlocks)));
    HostSd::openCount = 0;
    initialResolved = false;
    assert(!loadCompleteRuntimeTable(&missingUnlocksSd,
                                     releaseFixtureManifest(missingUnlocks),
                                     missingUnlocksReader, 0, 0, config,
                                     &initial, &initialResolved));
    assert(initialResolved);
    assert(HostSd::openCount == 1);

    AssetData::RuntimeManifest mismatched = releaseFixtureManifest(startup);
    ++mismatched.fileSize;
    initialResolved = true;
    assert(!loadCompleteRuntimeTable(&sd, mismatched, reader,
                                     0, 0, config, &initial, &initialResolved));
    assert(!initialResolved);
    PetBehaviorConfig unpublished = {};
    unpublished.schemaFingerprint = 0xA5A5A5A5UL;
    assert(!parseRuntimeTableBehavior(legacy.data(), legacy.size(),
                                      fixtureManifest(legacy), 1, 1, unpublished));
    assert(unpublished.schemaFingerprint == 0xA5A5A5A5UL);
    // Both backend read-back and this host reader consume the exact runtime
    // from a real backend-exported bundle, rather than a similar hand model.
    const std::vector<uint8_t> applied = readFixture(argv[4]);
    assert(parseRuntimeTableBehavior(applied.data(), applied.size(),
                                     releaseFixtureManifest(applied), 1, 1, config));
    assert(config.screenBlockCount == 9);
    assert(config.screenBlocks[0].kind == ScreenBlockKind::Animation);
    assert(config.screenBlocks[0].source == 0);
    assert(config.screenBlocks[0].x == 0 && config.screenBlocks[0].y == 32);
    assert(config.screenBlocks[0].width == 128 && config.screenBlocks[0].height == 96);
    for (uint8_t slot = 1; slot <= 8; ++slot)
    {
        const ScreenBlockConfig &block = config.screenBlocks[slot];
        assert(block.kind == ScreenBlockKind::Button && block.source == slot);
        assert(block.x == ((slot - 1) % 4) * 32);
        assert(block.y == (slot <= 4 ? 0 : 128));
    }
    const std::vector<uint8_t> sequence = readFixture(argv[argc - 2]);
    auto sequenceManifest = fixtureManifest(sequence);
    std::memcpy(sequenceManifest.bundleId.bytes, sequence.data() + 28, 16);
    assert(parseRuntimeTableBehavior(sequence.data(), sequence.size(),
                                     sequenceManifest, 1, 1, config));
    assert(config.actions[0].followingAnimationCount == 3);
    const uint8_t start = config.actions[0].firstFollowingAnimation;
    assert(config.followingAnimations[start].playbackCount == 2);
    assert(config.followingAnimations[start + 1].playbackCount == 1);
    assert(config.followingAnimations[start + 2].playbackCount == 5);
    assert(config.followingAnimations[start].animation.animationId ==
           config.followingAnimations[start + 2].animation.animationId);
    assert(config.followingAnimations[start].animation.animationId !=
           config.followingAnimations[start + 1].animation.animationId);
    for (uint8_t slot = 1; slot < config.actionCount; ++slot)
        assert(config.actions[slot].followingAnimationCount == 0);
    const std::vector<uint8_t> optional = readFixture(argv[argc - 1]);
    auto optionalManifest = fixtureManifest(optional);
    std::memcpy(optionalManifest.bundleId.bytes, optional.data() + 28, 16);
    assert(parseRuntimeTableBehavior(optional.data(), optional.size(),
                                     optionalManifest, 1, 1, config));
    assert(config.actionCount == 5);
    assert(config.actions[0].mode == PetBehaviorActionMode::Standard);
    assert(config.actions[1].mode == PetBehaviorActionMode::ConditionalAnimation);
    assert(!config.actions[1].hasFallbackAnimation);
    assert(config.actions[2].mode == PetBehaviorActionMode::RandomOutcome);
    for (uint8_t slot = 0; slot < 3; ++slot)
    {
        const auto &action = config.actions[slot];
        const auto &primary = config.actionOutcomes[action.firstOutcome].animationPlayback;
        assert(!primary.animation.valid() && primary.playbackCount == 0);
        assert(action.followingAnimationCount == 1);
        const auto &following = config.followingAnimations[action.firstFollowingAnimation];
        assert(following.animation.valid() && following.playbackCount == 2);
    }
    const auto &emptyCondition = config.actionConditions[config.actions[1].firstCondition];
    assert(!emptyCondition.animationPlayback.animation.valid());
    assert(emptyCondition.animationPlayback.playbackCount == 0);
    const auto &animatedCondition = config.actionConditions[config.actions[1].firstCondition + 1];
    assert(animatedCondition.animationPlayback.animation.valid());
    assert(animatedCondition.animationPlayback.playbackCount == 2);
    const auto &animatedOutcome = config.actionOutcomes[config.actions[2].firstOutcome + 1];
    assert(animatedOutcome.animationPlayback.animation.valid());
    assert(animatedOutcome.animationPlayback.playbackCount == 3);
    // Backend layout tests own geometry, source and uniqueness validation.
    // The reader still rejects rule ranges beyond storage, capacity and missing sections.
    const bool expectedAcceptance[] = {true, true, true, false, true, false, false};
    assert(argc - 7 == static_cast<int>(sizeof(expectedAcceptance) / sizeof(bool)));
    for (int argument = 5; argument < argc - 2; ++argument)
    {
        const std::vector<uint8_t> invalid = readFixture(argv[argument]);
        unpublished.screenBlockCount = 0xAA;
        const uint32_t fingerprintBefore = unpublished.schemaFingerprint;
        const bool accepted = parseRuntimeTableBehavior(invalid.data(), invalid.size(),
            releaseFixtureManifest(invalid), 1, 1, unpublished);
        assert(accepted == expectedAcceptance[argument - 5]);
        if (!accepted)
        {
            assert(unpublished.screenBlockCount == 0xAA);
            assert(unpublished.schemaFingerprint == fingerprintBefore);
        }
    }
#elif RUNTIME_TABLE_FULL_FEATURE
    assert(argc == 5);
    const std::vector<uint8_t> fixture = readFixture(argv[1]);
    PetBehaviorConfig config = {};
    const AssetData::RuntimeManifest manifest = releaseFixtureManifest(fixture);
    assert(parseRuntimeTableBehavior(fixture.data(), fixture.size(), manifest, 1, 1, config));
    assert(config.petStateCount == 5);
    assert(config.petStates[0].idleAnimation.valid());
    assert(config.idleAnimation.valid());
    assert(config.layoutUnselected.valid() && config.layoutSelected.valid());
    testOutfitSelectionReleaseFixture(fixture);
    for (int index = 2; index < argc; ++index)
        testInvalidAppearanceFixture(readFixture(argv[index]));
#elif RUNTIME_TABLE_VISUAL_CONTEXT
    assert(argc == 3);
    testValidFixtureLoads(readFixture(argv[1]), argv[1]);
    testInvalidFixtureFailsWithoutPartialPublication(readFixture(argv[2]));
#else
    assert(argc == 5);
    testValidFixtureLoads(readFixture(argv[1]), argv[1]);
    testInvalidFixtureFailsWithoutPartialPublication(readFixture(argv[2]));
    testInvalidFixtureFailsWithoutPartialPublication(readFixture(argv[3]));
    testInvalidFixtureFailsWithoutPartialPublication(readFixture(argv[4]));
#endif
    return 0;
}
