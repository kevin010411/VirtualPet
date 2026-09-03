#include <assert.h>
#include <fstream>
#include <iterator>
#include <vector>

#include "commands/domain/StatusSetContract.h"
#include "commands/domain/SystemCommandCatalog.h"
#include "appearance/domain/RuntimeTableAppearance.h"
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

namespace
{
std::vector<uint8_t> readFixture(const char *path)
{
    std::ifstream input(path, std::ios::binary);
    assert(input);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(input),
                                std::istreambuf_iterator<char>());
}

AssetData::RuntimeManifest fixtureManifest()
{
    AssetData::RuntimeManifest manifest = {};
    const uint8_t bundleId[16] = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    for (uint8_t index = 0; index < sizeof(bundleId); ++index)
        manifest.bundleId.bytes[index] = bundleId[index];
    return manifest;
}

AssetData::RuntimeManifest releaseFixtureManifest()
{
    AssetData::RuntimeManifest manifest = fixtureManifest();
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
    if (condition.source == StatusConditionSource::StageDays)
    {
        value = static_cast<int32_t>(context.stageDays);
        return true;
    }
    if (condition.source != StatusConditionSource::PetStat || condition.statSlot >= 10)
        return false;
    value = context.stats->values[condition.statSlot];
    return true;
}

void testBehaviorFullFixture(const std::vector<uint8_t> &fixture)
{
    PetBehaviorConfig config = {};
    const AssetData::RuntimeManifest manifest = fixtureManifest();
    assert(parseRuntimeTableBehavior(fixture.data(), fixture.size(), manifest, 1, 1, config));
    assert(config.schemaFingerprint == 0x12345678UL);
    assert(config.statCount == 10);
    assert(config.actionCount == 8);
    assert(config.actionConditionCount == 8);
    assert(config.statusSets.count == 2);
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
    assert(!resolution.playOnce);
    assert(resolution.requiredFrames == 24);
    assert(resolution.animation.animationId == 32);
}

void testInvalidFixtureFailsWithoutPartialPublication(const std::vector<uint8_t> &fixture)
{
    PetBehaviorConfig config = {};
    config.schemaFingerprint = 0xa5a5a5a5UL;
    config.statCount = 7;
    const AssetData::RuntimeManifest manifest = fixtureManifest();
    assert(!parseRuntimeTableBehavior(fixture.data(), fixture.size(), manifest, 1, 1, config));
    assert(config.schemaFingerprint == 0xa5a5a5a5UL);
    assert(config.statCount == 7);
}

void testValidFixtureLoads(const std::vector<uint8_t> &fixture)
{
    PetBehaviorConfig config = {};
    assert(parseRuntimeTableBehavior(
        fixture.data(), fixture.size(), fixtureManifest(), 1, 1, config));
}

void testOutfitSelectionReleaseFixture(const std::vector<uint8_t> &fixture)
{
    assert(findCompiledSystemCommand(RuntimeSystemCommandId::ChangeSpecies) == nullptr);
    SdFat sd(fixture.data(), fixture.size());
    uint8_t scratch[AssetData::kIoScratchBytes] = {};
    BundleReader reader(&sd, scratch, sizeof(scratch));
    const AssetData::RuntimeManifest manifest = releaseFixtureManifest();

    PetBehaviorConfig config = {};
    assert(parseRuntimeTableBehavior(fixture.data(), fixture.size(), manifest, 1, 1, config));

    uint8_t species[8] = {};
    size_t speciesCount = 0;
    assert(loadRuntimeTableSpecies(&sd, manifest, reader, species, 8, speciesCount));
    assert(speciesCount == 2 && species[0] == 1 && species[1] == 2);

    AppearanceSelection initial = {};
    assert(loadRuntimeTableInitialAppearance(&sd, manifest, reader, initial));
    assert(initial.speciesSlot == 1 && initial.outfitSlot == 1);

    ActivePetBehaviorStatSlots activeSlots(config);
    PetStatSnapshot stats = {};
    stats.speciesSlot = 1;
    stats.outfitSlot = 7;
    stats.customStats[0] = 50;

    uint8_t unlockMask = 0;
    assert(resolveRuntimeTableOutfitUnlockMask(
        &sd, manifest, reader, 1, activeSlots, stats, 0, true, unlockMask));
    assert(unlockMask == 0xE3U);

    uint8_t outfits[8] = {};
    size_t outfitCount = 0;
    assert(loadRuntimeTableOutfits(
        &sd, manifest, reader, 1, unlockMask, outfits, 8, outfitCount));
    const uint8_t expectedVisible[] = {1, 2, 4, 5, 6, 7, 8};
    assert(outfitCount == sizeof(expectedVisible));
    for (size_t index = 0; index < outfitCount; ++index)
        assert(outfits[index] == expectedVisible[index]);

    OutfitPreview locked = {};
    assert(findRuntimeTableOutfitPreview(&sd, manifest, reader, 1, 4, true, locked));
    assert(locked.speciesSlot == 1 && locked.outfitSlot == 4 && locked.animation.valid());

    stats.stage_days = 10;
    assert(resolveRuntimeTableOutfitUnlockMask(
        &sd, manifest, reader, 1, activeSlots, stats, unlockMask, false, unlockMask));
    assert(unlockMask == 0xFBU);
    stats.stage_days = 0;
    stats.customStats[0] = 0;
    assert(resolveRuntimeTableOutfitUnlockMask(
        &sd, manifest, reader, 1, activeSlots, stats, unlockMask, false, unlockMask));
    assert(unlockMask == 0xFBU);

    uint8_t resetMask = 0;
    assert(resolveRuntimeTableOutfitUnlockMask(
        &sd, manifest, reader, 1, activeSlots, stats, 0, true, resetMask));
    assert(resetMask == 0xC3U);
}

void testInvalidAppearanceFixture(const std::vector<uint8_t> &fixture)
{
    SdFat sd(fixture.data(), fixture.size());
    uint8_t scratch[AssetData::kIoScratchBytes] = {};
    BundleReader reader(&sd, scratch, sizeof(scratch));
    PetBehaviorConfig published = {};
    published.schemaFingerprint = 0xA5A5A5A5UL;
    published.statCount = 7;
    PetBehaviorConfig candidate = published;
    const AssetData::RuntimeManifest manifest = releaseFixtureManifest();
    bool accepted = parseRuntimeTableBehavior(
        fixture.data(), fixture.size(), manifest, 1, 1, candidate);
    if (accepted)
    {
        ActivePetBehaviorStatSlots activeSlots(candidate);
        PetStatSnapshot stats = {};
        stats.speciesSlot = 1;
        stats.outfitSlot = 1;
        accepted = validateRuntimeTableAppearance(
            &sd, manifest, reader, activeSlots, stats);
    }
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
#if RUNTIME_TABLE_FULL_FEATURE
    assert(argc == 10);
    PetBehaviorConfig config = {};
    const AssetData::RuntimeManifest manifest = fixtureManifest();
    const std::vector<uint8_t> fixture = readFixture(argv[1]);
    assert(parseRuntimeTableBehavior(fixture.data(), fixture.size(), manifest, 1, 1, config));
    assert(config.idleTriggerCount == 0);
#if ENABLE_GUESS_GAME
    assert(config.guessEffectCount == 1);
    assert(config.guessEffects[0].active);
    assert(config.guessEffects[0].outcome == PetBehaviorGuessOutcome::RoundCorrect);
    assert(config.guessEffects[0].statSlot == 0);
    assert(config.guessEffects[0].operation == PetBehaviorEffectOperation::Change);
    assert(config.guessEffects[0].value == 1);
#endif
    testOutfitSelectionReleaseFixture(readFixture(argv[2]));
    for (int index = 3; index < argc; ++index)
        testInvalidAppearanceFixture(readFixture(argv[index]));
#else
    assert(argc == 6);
    testBehaviorFullFixture(readFixture(argv[1]));
    testInvalidFixtureFailsWithoutPartialPublication(readFixture(argv[2]));
    testInvalidFixtureFailsWithoutPartialPublication(readFixture(argv[3]));
    testValidFixtureLoads(readFixture(argv[4]));
    testInvalidFixtureFailsWithoutPartialPublication(readFixture(argv[5]));
#endif
    return 0;
}
