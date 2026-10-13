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
uint16_t Renderer::versionCountFor(const AssetData::AnimationRef &) { return 1; }
unsigned long Renderer::frameIntervalFor(const AssetData::AnimationRef &ref, uint8_t version, unsigned long) { return metadata(ref, version).frameMs; }
SdFat *Renderer::sdCard() const { return nullptr; }

int main(int argc, char **argv)
{
    assert(argc == 3 || argc == 4);
    const auto bytes = readBytes(argv[1]);
    const bool silentFixture = argc == 4;
    if (!silentFixture) pcm = readBytes(argv[2]);
    const auto root = std::filesystem::path(argv[1]).parent_path();
    speciesPack = readBytes((root / "assets/species_1.data").string().c_str());
    sharedPack = readBytes((root / "assets/shared.data").string().c_str());
    bundleId = manifest(bytes).bundleId;
    PetBehaviorConfig config = {};
    assert(parseRuntimeTableBehavior(bytes.data(), bytes.size(), manifest(bytes), 1, 1, config));
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
    assert(pet.actionCount(0) == 1);
    tick(controller, 1000);
    if (silentFixture)
    {
        assert(starts == 0 && !amplifier);
        controller.cancelAll();
        assert(pet.customStat(0) == 60 && pet.actionCount(0) == 1);
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
    assert(pet.actionCount(0) == 1);
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
        assert(failedPet.customStat(0) == 60 && failedPet.actionCount(0) == 1);
        tick(failedController, origin + 1000);
        assert(!failedController.isBusy());
        assert(failedPet.customStat(0) == 60 && failedPet.actionCount(0) == 1);
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
    puts("[PASS] Real Web runtime19/audio.data; action effects once, first frame before samples, absolute frame catch-up, repeats, short/long audio, late EOF/read failure, cycles, cancel and sleep gate");
}
