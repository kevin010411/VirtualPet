#include <assert.h>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#include "commands/domain/StatusSetContract.h"
#include "commands/domain/SystemCommandCatalog.h"
#include "appearance/domain/RuntimeTableAppearance.h"
#include "appearance/adapters/SdAppearanceLoader.h"
#include "pet_behavior/domain/PetBehaviorRuntimeRules.h"
#include "pet_behavior/domain/RuntimeTableBehavior.h"

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

struct StatusContext
{
    const PetBehaviorStatValues *stats;
    uint32_t stageDays;
};

bool statusValue(const StatusSetCondition &condition,
                 const void *rawContext,
                 int32_t &value)
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

    uint8_t species[8] = {};
    size_t speciesCount = 0;
    assert(loadRuntimeTableSpecies(&sd, manifest, species, 8, speciesCount));
    assert(speciesCount == 2 && species[0] == 1 && species[1] == 2);

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

    uint8_t species[8] = {};
    size_t speciesCount = 0;
    HostSd::openCount = 0;
    assert(loader.loadSpecies(species, 8, speciesCount));
    assert(speciesCount == 2 && HostSd::openCount == 1);

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
    speciesCount = 0;
    assert(!invalidLoader.loadSpecies(species, 8, speciesCount));
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

int main(int argc, char **argv)
{
    AssetData::AssetFrameAddress ninthSpecies = {};
    ninthSpecies.speciesSlot = 9;
    ninthSpecies.outfitSlot = 1;
    ninthSpecies.animationId = 1;
    assert(AssetData::isValidFrameAddress(ninthSpecies));
    ninthSpecies.outfitSlot = 9;
    assert(!AssetData::isValidFrameAddress(ninthSpecies));
#if RUNTIME_TABLE_V7
    assert(argc == 4);
    const std::vector<uint8_t> valid = readFixture(argv[1]);
    const std::vector<uint8_t> legacy = readFixture(argv[2]);
    const std::vector<uint8_t> startup = readFixture(argv[3]);
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
