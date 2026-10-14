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
    const bool idleFixture = argc == 4 && strcmp(argv[3], "--idle") == 0;
    const bool animationFixture = argc == 4 && strcmp(argv[3], "--animation") == 0;
    const bool silentFixture = argc == 4 && !idleFixture && !animationFixture;
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
        controller.setBaseAnimation(primary, 1, {});
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
        puts("[PASS] v21 animation BGM: shared context, loop/version rotation, per-repeat and following audio, cutoff, short EOF, effects once, bounded SD retry and cancellation");
        return 0;
    }
    if (idleFixture)
    {
        assert(config.petStateCount == 2 && config.idleAudio[0].bound() && config.idleAudio[1].bound());
        assert(config.idleAudio[0].offset != config.idleAudio[1].offset);
        Renderer renderer(nullptr, nullptr);
        AnimationController controller(renderer);
        controller.configureRuntimeContract(config);
        controller.setup(config.idleAnimation);
        Pet pet;
        PetBehaviorRuntime runtime(config, pet, controller, renderer);
        runtime.initializeStats();
        auto refresh = [&]() {
            controller.setBaseAnimation(runtime.baseAnimation(), runtime.activePetState().slot, runtime.baseAudio());
        };
        pet.setCustomStat(0, int16_t(config.petStates[0].predicate.minimum));
        assert(runtime.activePetState().slot == 0);
        refresh();
        tick(controller, 1000);
        assert(starts == 1 && amplifier && !controller.isBusy() && !controller.hasFinitePlayback());
        const auto queries = versionQueries;
        tick(controller, 1150);
        assert(drawnFrame == 2 && starts == 1);
        for (uint32_t loop = 1; loop <= 10; ++loop)
        {
            tick(controller, 1000 + loop * 200);
            assert(starts == loop + 1 && drawnFrame == 1 && amplifier);
            assert(versionQueries == queries + (loop == 10 ? 1 : 0));
        }
        const auto stateZero = runtime.baseAnimation();
        pet.setCustomStat(0, int16_t(config.petStates[1].predicate.minimum));
        assert(runtime.activePetState().slot == 1);
        assert(runtime.baseAnimation().animationId == stateZero.animationId);
        refresh();
        assert(!amplifier); // Same animation, different State identity stops immediately.
        tick(controller, 3050);
        assert(amplifier && starts == 12 && preparedOffset == config.idleAudio[1].offset && drawnFrame == 1);
        // Even identical resource refs restart when State identity changes.
        controller.setBaseAnimation(stateZero, 0, config.idleAudio[1]);
        assert(!amplifier);
        tick(controller, 3100);
        assert(starts == 13 && drawnFrame == 1);
        pet.setCustomStat(0, 1);
        assert(runtime.activePetState().isDefault);
        refresh();
        assert(!amplifier);
        tick(controller, 3200);
        assert(amplifier == config.defaultIdleAudio.bound());
        // A short clip remains silent until the full visual loop boundary.
        SequenceAudioRef shortAudio = config.idleAudio[0];
        shortAudio.sampleCount = 8;
        controller.setBaseAnimation(stateZero, 0, shortAudio);
        tick(controller, 4000);
        const auto shortStart = starts;
        tick(controller, 4150);
        assert(!amplifier && starts == shortStart && drawnFrame == 2);
        tick(controller, 4200);
        assert(amplifier && starts == shortStart + 1 && drawnFrame == 1);
        // Natural SD preparation errors do not retry at every update.
        failRead = true;
        controller.setBaseAnimation(stateZero, 1, config.idleAudio[1]);
        tick(controller, 5000);
        const auto failedPrepare = preparations;
        tick(controller, 5050);
        tick(controller, 5150);
        assert(preparations == failedPrepare && !amplifier && drawnFrame == 2);
        tick(controller, 5200);
        assert(preparations == failedPrepare + 1);
        failRead = false;
        controller.cancelAll();
        tick(controller, 6000);
        assert(amplifier);
        Animation fixed(stateZero, 100, false, 1, FirmwarePlaybackRole::Status);
        AnimationSequence status(&fixed, 1);
        assert(controller.replace(status) == PlaybackResult::Accepted);
        assert(!amplifier);
        tick(controller, 6010);
        assert(controller.isBusy() && !amplifier);
        tick(controller, 6110);
        assert(!controller.isBusy() && amplifier && drawnFrame == 1);
        // Action replacement similarly stops idle; completion resumes from zero.
        assert(runtime.executeAction(0) == PetBehaviorActionResult::Applied);
        assert(!amplifier);
        for (uint32_t t = 6200; t <= 7200; t += 100) tick(controller, t);
        assert(!controller.isBusy());
        refresh();
        controller.setBaseAnimation(stateZero, 0, config.idleAudio[0]);
        tick(controller, 8000);
        const auto beforeDay = pet.stageDays();
        assert(runtime.advancePetDay() && pet.stageDays() == beforeDay + 1);
        assert(!controller.isBusy() && !controller.hasFinitePlayback());
        PowerController power;
        power.interact(8000);
        assert(power.shouldSleep(130000, controller.hasFinitePlayback()));
        controller.cancelAll(); // Menu/STOP cancellation seam.
        assert(!amplifier);
        const auto beforeWake = starts;
        tick(controller, 9000);
        assert(starts == beforeWake + 1 && amplifier && drawnFrame == 1);
        pet.configureDeath(true, 1);
        pet.setCustomStat(0, 0);
        assert(pet.evaluateDeath() && !runtime.baseAudio().bound());
        controller.cancelAll();
        refresh();
        tick(controller, 10000);
        assert(!amplifier && !controller.isBusy());
        puts("[PASS] Real Web idle bindings, shared-animation State identity, default, full-loop audio, ten-loop Version selection, short EOF, bounded SD retry, Action/Status return, daily changes, STOP and death");
        return 0;
    }
    uint8_t slot = 0;
    while (slot < config.actionCount && !config.actionAudio[slot].bound()) ++slot;
    if (silentFixture) { assert(slot == config.actionCount); slot = 0; }
    else assert(slot < config.actionCount);
    Renderer renderer(nullptr, nullptr);
    AnimationController controller(renderer);
    controller.configureRuntimeContract(config);
    controller.setup(config.idleAnimation);
    Pet pet;
    PetBehaviorRuntime runtime(config, pet, controller, renderer);
    runtime.initializeStats();
    pet.setCustomStat(0, 20); // Make the fixture's +40 effect observable, unclamped.
    config.trackedActionCount = 1;
    config.trackedActions[0] = slot;
    pet.configureTrackedActions(1);
    const auto before = pet.statSnapshot();
    assert(runtime.executeAction(slot) == PetBehaviorActionResult::Applied);
    const auto after = pet.statSnapshot();
    assert(after.customStats[0] == 60 && before.customStats[0] == 20);
    assert(pet.actionCount(0) == expectedTrackedActionCount);
    tick(controller, 1000);
    if (silentFixture)
    {
        assert(starts == 0 && !amplifier);
        controller.cancelAll();
        assert(pet.customStat(0) == 60 && pet.actionCount(0) == expectedTrackedActionCount);
        puts("[PASS] Real silent/muted/feature-off Web fixture executes effects once without sample playback");
        return 0;
    }
    assert(starts == 1 && amplifier && controller.hasFinitePlayback());
    const uint16_t primary = drawnAnimation;
    tick(controller, 1150);
    assert(drawnFrame == 2); // Uses actual exported two-frame/100ms metadata.
    assert(starts == 1);
    tick(controller, 1200);
    assert(starts == 1); // Segment repetitions never restart audio.
    tick(controller, 1750);
    assert(starts == 1);
    assert(memcmp(after.customStats, pet.statSnapshot().customStats, sizeof(after.customStats)) == 0);
    assert(pet.actionCount(0) == expectedTrackedActionCount);
    (void)before; (void)primary;
    controller.cancelAll();
    assert(!controller.isBusy() && !amplifier);

    const auto realAnimation = config.actionOutcomes[config.actions[slot].firstOutcome].animationPlayback.animation;
    Animation items[2] = {Animation::complete(realAnimation, 2), Animation::complete(realAnimation, 1)};
    AnimationSequence sequence(items, 2);
    sequence.audio = config.actionAudio[slot];
    assert(controller.replace(sequence) == PlaybackResult::Accepted);
    tick(controller, 2000);
    const unsigned started = starts;
    tick(controller, 2580);
    assert(starts == started && controller.isBusy());
    tick(controller, 2600);
    assert(!controller.isBusy());
    assert(!amplifier); // Longer audio stops with its visual sequence.

    // Natural EOF discovered late preserves the original monotonic origin.
    sequence.audio.sampleCount = 8;
    assert(controller.replace(sequence) == PlaybackResult::Accepted);
    tick(controller, 3000);
    tick(controller, 3500);
    assert(controller.isBusy());
    tick(controller, 3600);
    assert(!controller.isBusy());
    sequence.audio = config.actionAudio[slot];
    assert(controller.replace(sequence) == PlaybackResult::Accepted);
    tick(controller, 4000);
    failRead = true;
    tick(controller, 4500);
    assert(!amplifier && controller.isBusy());
    assert(controller.audioError() == SequenceAudioError::ReadFailed);
    tick(controller, 4600);
    assert(!controller.isBusy());
    failRead = false;

    // Whole sequence looping, finite sleep gate, STOP-like cancellation.
    sequence.loop = true;
    assert(controller.replace(sequence) == PlaybackResult::Accepted);
    tick(controller, 5000);
    const unsigned loopingStart = starts;
    assert(!controller.hasFinitePlayback());
    tick(controller, 5600);
    assert(starts == loopingStart + 1);
    PowerController power;
    power.interact(5000);
    assert(power.shouldSleep(130000, controller.hasFinitePlayback()));
    controller.cancelAll();
    assert(!amplifier && !controller.isBusy());
    tick(controller, 5700);
    assert(!amplifier); // Wake redraw cannot resume an old sample position.
    // The same public Action execution seam retains commits under each
    // distinct playback failure, including preparation and hardware start.
    auto failureCase = [&](SequenceAudioError expected, uint32_t origin) {
        AnimationController failedController(renderer);
        failedController.configureRuntimeContract(config);
        failedController.setup(config.idleAnimation);
        Pet failedPet;
        PetBehaviorRuntime failedRuntime(config, failedPet, failedController, renderer);
        failedRuntime.initializeStats();
        failedPet.setCustomStat(0, 20);
        failedPet.configureTrackedActions(1);
        const bool duringPlayback = expected == SequenceAudioError::Underrun;
        failRead = expected == SequenceAudioError::ReadFailed;
        failStart = expected == SequenceAudioError::HardwareFailed;
        assert(failedRuntime.executeAction(slot) == PetBehaviorActionResult::Applied);
        tick(failedController, origin);
        if (duringPlayback) underrun = true;
        tick(failedController, origin + 750);
        assert(!amplifier && failedController.isBusy());
        assert(failedController.audioError() == expected);
        assert(failedPet.customStat(0) == 60 && failedPet.actionCount(0) == expectedTrackedActionCount);
        tick(failedController, origin + 1000);
        assert(!failedController.isBusy());
        assert(failedPet.customStat(0) == 60 && failedPet.actionCount(0) == expectedTrackedActionCount);
        failRead = failStart = underrun = false;
    };
    failureCase(SequenceAudioError::ReadFailed, 6000);
    failureCase(SequenceAudioError::HardwareFailed, 8000);
    failureCase(SequenceAudioError::Underrun, 10000);
    sequence.loop = false;
    assert(controller.replace(sequence) == PlaybackResult::Accepted);
    tick(controller, 12000);
    assert(amplifier);
    AnimationSequence silentReplacement(items, 1);
    assert(controller.replace(silentReplacement) == PlaybackResult::Accepted);
    assert(!amplifier); // Accepted replacement clears the old audio and tail queue.
    puts("[PASS] Real Web runtime20/audio.data; action effects once, first frame before samples, absolute frame catch-up, repeats, short/long audio, late EOF/read failure, cycles, cancel and sleep gate");
}
