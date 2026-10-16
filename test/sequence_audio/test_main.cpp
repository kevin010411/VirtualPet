#include <assert.h>
#include <fstream>
#include <iterator>
#include <vector>
#include <string.h>
#include <stdio.h>
#include <filesystem>
#include "animation/AnimationController.h"
#include "display/Renderer.h"
#include "pet/Pet.h"
#include "pet/PetBehaviorRuntime.h"
#include "resources/RuntimeTableBehavior.h"
#include "resources/detail/RuntimeTableReader.h"
#include "platform/PowerController.h"

unsigned long hostMillis = 0;
constexpr uint16_t expectedTrackedActionCount = ENABLE_COLLECTION_CATALOG ? 1 : 0;
namespace
{
std::vector<uint8_t> pcm;
std::vector<uint8_t> speciesPack, sharedPack;
AssetData::BundleId bundleId;
bool outputRunning = false, amplifier = false, prepared = false;
bool failRead = false;
bool failStart = false, underrun = false;
SequenceAudioError outputError = SequenceAudioError::None;
uint32_t outputStarted = 0, outputSamples = 0, outputLength = 0;
unsigned starts = 0, fades = 0, rendered = 0;
unsigned preparations = 0, versionQueries = 0;
uint32_t preparedOffset = 0;
uint16_t drawnFrame = 0, drawnAnimation = 0;
std::vector<uint8_t> readBytes(const char *path)
{
    std::ifstream file(path, std::ios::binary);
    assert(file);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
AssetData::RuntimeManifest manifest(const std::vector<uint8_t> &bytes)
{
    AssetData::RuntimeManifest m = {};
    memcpy(m.bundleId.bytes, bytes.data() + 28, 16);
    m.fileSize = bytes.size();
    m.schemaFingerprint = RuntimeTableInternal::readU32(bytes.data() + 44);
    m.fileCrc32 = RuntimeTableInternal::readU32(bytes.data() + 48);
    return m;
}
void tick(AnimationController &controller, uint32_t time)
{
    hostMillis = time;
    controller.preparePlayback(time);
    controller.tick(time);
}
AssetData::AnimationRecord metadata(const AssetData::AnimationRef &ref, uint8_t version)
{
    const auto &pack = ref.shared() ? sharedPack : speciesPack;
    HostSd::mountedData = pack.data();
    HostSd::mountedSize = pack.size();
    SdFat sd;
    uint8_t scratch[512] = {};
    BundleReader reader(&sd, scratch, sizeof(scratch));
    assert(reader.configureBundle(bundleId));
    AssetData::AnimationRecord record;
    AssetData::AssetFrameAddress address = {ref.speciesSlot, ref.outfitSlot, ref.animationId, version, 0};
    assert(reader.resolveAnimation(address, record));
    return record;
}
}
namespace SequenceAudioPlatform
{
bool prepare(SdFat *, const SequenceAudioRef &ref)
{
    ++preparations;
    preparedOffset = ref.offset;
    stop(false);
    outputError = SequenceAudioError::None;
    prepared = !failRead && ref.offset <= pcm.size() && ref.sampleCount <= pcm.size() - ref.offset;
    if (!prepared) outputError = SequenceAudioError::ReadFailed;
    outputLength = ref.sampleCount;
    outputSamples = 0;
    return prepared;
}
bool start()
{
    assert(rendered > 0); // The first display transfer precedes hardware start.
    ++starts;
    outputStarted = hostMillis;
    outputRunning = amplifier = prepared && !failStart;
    return prepared && !failStart;
}
void service()
{
    if (!outputRunning) return;
    outputSamples = (hostMillis - outputStarted) * 8;
    if (failRead || underrun)
    {
        outputError = underrun ? SequenceAudioError::Underrun : SequenceAudioError::ReadFailed;
        outputRunning = amplifier = false;
    }
    else if (outputSamples >= outputLength)
    {
        outputSamples = outputLength; // EOF was earlier than this late service.
        outputRunning = amplifier = false;
    }
}
uint32_t consumedSamples() { return outputSamples; }
bool active() { return outputRunning; }
SequenceAudioError error() { return outputError; }
void stop(bool fade)
{
    if (fade && outputRunning) ++fades;
    outputRunning = false; // Sample engine stops before the amplifier.
    amplifier = false;
}
}
Renderer::Renderer(Adafruit_ST7735 *, SdFat *) {}
Renderer::~Renderer() = default;
void Renderer::initAnimations() {}
bool Renderer::setAnimation(const AssetData::AnimationRef &ref, uint8_t, bool) { return ref.valid(); }
bool Renderer::ShowAnimationFrame(const AssetData::AnimationRef &ref, uint8_t, uint16_t frame,
                                 int, int, int, uint16_t, uint16_t)
{
    ++rendered;
    drawnAnimation = ref.animationId;
    drawnFrame = frame;
    return true;
}
bool Renderer::advanceAnimationFrame() { ++rendered; return true; }
bool Renderer::animationFrameFailed() const { return false; }
bool Renderer::willRestartAnimationLoop() const { return false; }
uint16_t Renderer::frameCountFor(const AssetData::AnimationRef &ref, uint8_t version) { return metadata(ref, version).frameCount; }
uint16_t Renderer::versionCountFor(const AssetData::AnimationRef &) { ++versionQueries; return 1; }
unsigned long Renderer::frameIntervalFor(const AssetData::AnimationRef &ref, uint8_t version, unsigned long) { return metadata(ref, version).frameMs; }
SdFat *Renderer::sdCard() const { return nullptr; }

int main(int argc, char **argv)
{
    assert(argc == 3 || argc == 4);
    const auto bytes = readBytes(argv[1]);
    const bool animationFixture = argc == 4 && strcmp(argv[3], "--animation") == 0;
    const bool silentFixture = argc == 4 && !animationFixture;
    if (!silentFixture) pcm = readBytes(argv[2]);
    const auto root = std::filesystem::path(argv[1]).parent_path();
    speciesPack = readBytes((root / "assets/species_1.data").string().c_str());
    sharedPack = readBytes((root / "assets/shared.data").string().c_str());
    bundleId = manifest(bytes).bundleId;
    PetBehaviorConfig config = {};
    assert(parseRuntimeTableBehavior(bytes.data(), bytes.size(), manifest(bytes), 1, 1, config));
    if (animationFixture)
    {
        const auto primary = config.idleAnimation;
        const auto following = config.followingAnimations[config.actions[0].firstFollowingAnimation].animation;
        auto &primaryAudio = config.animationAudio[primary.animationId - 1];
        const auto followingAudio = config.animationAudio[following.animationId - 1];
        assert(primaryAudio.bound() && followingAudio.bound());
        assert(primaryAudio.offset != followingAudio.offset);
        Renderer renderer(nullptr, nullptr);
        AnimationController controller(renderer);
        controller.configureRuntimeContract(config);
        controller.setup(primary);
        tick(controller, 1000);
        assert(starts == 1 && amplifier && !controller.isBusy());
        const auto selectedVersionQueries = versionQueries;
        // A state-only change using the same animation keeps its current loop.
        controller.setBaseAnimation(primary);
        tick(controller, 1100);
        assert(starts == 1 && drawnFrame == 2);
        for (uint32_t loop = 1; loop <= 10; ++loop) tick(controller, 1000 + loop * 200);
        assert(starts == 11 && versionQueries == selectedVersionQueries + 1);
        Animation items[] = {Animation::complete(primary, 2), Animation::complete(following, 1)};
        AnimationSequence sequence(items, 2);
        assert(controller.replace(sequence) == PlaybackResult::Accepted);
        tick(controller, 4000);
        assert(preparedOffset == primaryAudio.offset);
        const auto firstStart = starts;
        tick(controller, 4200);
        assert(starts == firstStart + 1 && preparedOffset == primaryAudio.offset);
        tick(controller, 4400);
        assert(starts == firstStart + 2 && preparedOffset == followingAudio.offset);
        tick(controller, 4600);
        assert(!controller.isBusy() && preparedOffset == primaryAudio.offset);
        // Short music remains silent until this animation completes a full loop.
        controller.cancelAll();
        const auto originalAudio = primaryAudio;
        primaryAudio.sampleCount = 8;
        controller.setBaseAnimation(primary);
        tick(controller, 5000);
        const auto shortStart = starts;
        tick(controller, 5150);
        assert(starts == shortStart && !amplifier);
        tick(controller, 5200);
        assert(starts == shortStart + 1 && amplifier);
        primaryAudio = originalAudio;
        controller.cancelAll();
        Pet pet;
        PetBehaviorRuntime runtime(config, pet, controller, renderer);
        runtime.initializeStats();
        pet.setCustomStat(0, 20);
        assert(runtime.executeAction(0) == PetBehaviorActionResult::Applied);
        const auto after = pet.statSnapshot();
        for (uint32_t time = 6000; time <= 7100; time += 100) tick(controller, time);
        assert(!controller.isBusy());
        assert(memcmp(after.customStats, pet.statSnapshot().customStats, sizeof(after.customStats)) == 0);
        // SD failures retain the visual timeline, with retry at a full loop only.
        controller.cancelAll();
        failRead = true;
        tick(controller, 8000);
        const auto failedPrepares = preparations;
        tick(controller, 8150);
        assert(!amplifier && preparations == failedPrepares);
        tick(controller, 8200);
        assert(preparations == failedPrepares + 1);
        failRead = false;
        controller.cancelAll();
        assert(!amplifier);
        Animation timed(primary, 450, false);
        AnimationSequence held(&timed, 1);
        assert(controller.replace(held) == PlaybackResult::Accepted);
        tick(controller, 10000);
        const auto timedStart = starts;
        tick(controller, 10200);
        tick(controller, 10400);
        assert(starts == timedStart + 2 && controller.isBusy());
        tick(controller, 10450);
        assert(!controller.isBusy());
        // Exercise current Action music failures without reverting committed effects.
        controller.cancelAll();
        auto failureCase = [&](SequenceAudioError expected, uint32_t origin) {
            AnimationController failed(renderer);
            failed.configureRuntimeContract(config);
            failed.setup(config.idleAnimation);
            Pet failedPet;
            PetBehaviorRuntime failedRuntime(config, failedPet, failed, renderer);
            failedRuntime.initializeStats();
            failedPet.setCustomStat(0, 20);
            failRead = expected == SequenceAudioError::ReadFailed;
            failStart = expected == SequenceAudioError::HardwareFailed;
            assert(failedRuntime.executeAction(0) == PetBehaviorActionResult::Applied);
            const auto committed = failedPet.statSnapshot();
            tick(failed, origin);
            underrun = expected == SequenceAudioError::Underrun;
            tick(failed, origin + 50);
            assert(!amplifier && failed.isBusy() && failed.audioError() == expected);
            assert(memcmp(committed.customStats, failedPet.statSnapshot().customStats,
                          sizeof(committed.customStats)) == 0);
            failRead = failStart = underrun = false;
            failed.cancelAll();
            assert(!amplifier && !failed.isBusy());
        };
        failureCase(SequenceAudioError::ReadFailed, 12000);
        failureCase(SequenceAudioError::HardwareFailed, 14000);
        failureCase(SequenceAudioError::Underrun, 16000);

        Animation loopItem = Animation::complete(primary);
        AnimationSequence looping(&loopItem, 1);
        looping.loop = true;
        assert(controller.replace(looping) == PlaybackResult::Accepted);
        tick(controller, 18000);
        const auto loopStarts = starts;
        tick(controller, 18200);
        assert(starts == loopStarts + 1 && !controller.hasFinitePlayback());
        PowerController power;
        power.interact(18000);
        assert(power.shouldSleep(150000, controller.hasFinitePlayback()));
        controller.cancelAll();
        assert(!amplifier && !controller.isBusy());
        puts("[PASS] v21 animation BGM: shared context, loop/version rotation, per-repeat and following audio, cutoff, short EOF, effects once, bounded SD retry and cancellation");
        return 0;
    }
    // Legacy action/idle fixtures no longer define production playback. Keep
    // silent export coverage through the same public Action execution seam.
    assert(silentFixture);
    const uint8_t slot = 0;
    Renderer renderer(nullptr, nullptr);
    AnimationController controller(renderer);
    controller.configureRuntimeContract(config);
    controller.setup(config.idleAnimation);
    Pet pet;
    PetBehaviorRuntime runtime(config, pet, controller, renderer);
    runtime.initializeStats();
    pet.setCustomStat(0, 20);
    config.trackedActionCount = 1;
    config.trackedActions[0] = slot;
    pet.configureTrackedActions(1);
    assert(runtime.executeAction(slot) == PetBehaviorActionResult::Applied);
    assert(pet.customStat(0) == 60 && pet.actionCount(0) == expectedTrackedActionCount);
    tick(controller, 1000);
    assert(starts == 0 && !amplifier);
    controller.cancelAll();
    assert(pet.customStat(0) == 60 && pet.actionCount(0) == expectedTrackedActionCount);
    puts("[PASS] Silent v21 Web fixture commits effects once without sample playback");
}
