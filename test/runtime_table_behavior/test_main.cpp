#include <assert.h>
#include <fstream>
#include <iterator>
#include <vector>

#include "commands/domain/StatusSetContract.h"
#include "pet_behavior/domain/PetBehaviorRuntimeRules.h"
#include "pet_behavior/domain/RuntimeTableBehavior.h"

namespace AssetData
{
// Behavior fixtures do not mount asset packs. The complete SD-loader tests own
// physical pack resolution; this seam only makes the behavior reader linkable.
bool animationReferenceExists(BundleReader &, const AnimationRef &, uint8_t)
{
    return false;
}
} // namespace AssetData

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
} // namespace

int main(int argc, char **argv)
{
    assert(argc == 4);
    testBehaviorFullFixture(readFixture(argv[1]));
    testInvalidFixtureFailsWithoutPartialPublication(readFixture(argv[2]));
    testInvalidFixtureFailsWithoutPartialPublication(readFixture(argv[3]));
    return 0;
}
