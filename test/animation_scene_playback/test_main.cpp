#include <assert.h>

#include <fstream>
#include <iterator>
#include <vector>

#include "animation/application/AnimationController.h"
#include "commands/application/CommandController.h"
#include "pet_behavior/domain/PetBehaviorTypes.h"
#include "pet_behavior/domain/RuntimeTableBehavior.h"
#include "presentation/adapters/rendering/Renderer.h"
#include "presentation/application/LayoutRenderer.h"

namespace
{
enum class RenderEventKind : uint8_t
{
    LayoutFrame,
    AnimationFrame,
};

struct RenderEvent
{
    RenderEventKind kind;
    uint16_t animationId;
    uint8_t version;
    uint16_t frame;
};

std::vector<RenderEvent> events;
uint16_t selectedAnimationId = 0;
uint8_t selectedVersion = 0;

AssetData::AnimationRef animation(uint16_t id) { return {1, 1, id}; }

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

class Host : public CommandHost
{
public:
    bool commandHasAnimation(FirmwarePlaybackRole) const override { return true; }
    bool commandCanStatus() const override { return true; }
    void commandStatus() override {}
};

void prepareAndTick(AnimationController &animations, LayoutRenderer &layout,
                    unsigned long now)
{
    animations.preparePlayback(now);
    layout.updatePlayback(
        animations.currentAnimation(), animations.currentVersionIndex());
    animations.tick(now);
}

void assertAtomicSceneBeforeFrame(uint8_t layoutVersion, uint16_t centerId)
{
    assert(events.size() == 9);
    for (size_t index = 0; index < 8; ++index)
    {
        assert(events[index].kind == RenderEventKind::LayoutFrame);
        assert(events[index].version == layoutVersion);
        assert(events[index].frame == index + 1);
    }
    assert(events.back().kind == RenderEventKind::AnimationFrame);
    assert(events.back().animationId == centerId);
}
} // namespace

namespace AssetData
{
bool sameBundleId(const BundleId &left, const BundleId &right)
{
    for (uint8_t index = 0; index < sizeof(left.bytes); ++index)
        if (left.bytes[index] != right.bytes[index])
            return false;
    return true;
}

bool animationReferenceExists(BundleReader &, const AnimationRef &, uint8_t)
{
    return true;
}
} // namespace AssetData

BundleReader::BundleReader(SdFat *sd, uint8_t *scratch, size_t scratchSize)
    : sd_(sd), scratch_(scratch), scratchSize_(scratchSize)
{
}

Renderer::Renderer(Adafruit_ST7735 *, SdFat *) {}
Renderer::~Renderer() = default;
void Renderer::initAnimations() {}
bool Renderer::setAnimation(
    const AssetData::AnimationRef &reference, uint8_t versionIndex, bool)
{
    selectedAnimationId = reference.animationId;
    selectedVersion = versionIndex;
    return true;
}
bool Renderer::advanceAnimationFrame()
{
    events.push_back({RenderEventKind::AnimationFrame, selectedAnimationId, selectedVersion, 1});
    return true;
}
bool Renderer::ShowAnimationFrame(
    const AssetData::AnimationRef &reference, uint8_t versionIndex,
    uint16_t frameIndex, int, int, int)
{
    events.push_back({RenderEventKind::LayoutFrame, reference.animationId, versionIndex, frameIndex});
    return true;
}
bool Renderer::willRestartAnimationLoop() const { return false; }
bool Renderer::animationFrameFailed() const { return false; }
uint16_t Renderer::frameCountFor(const AssetData::AnimationRef &, uint8_t)
{
    return 1;
}
uint16_t Renderer::versionCountFor(const AssetData::AnimationRef &) { return 1; }
unsigned long Renderer::frameIntervalFor(
    const AssetData::AnimationRef &, uint8_t, unsigned long)
{
    return 1;
}
SdFat *Renderer::sdCard() const { return nullptr; }

int main(int argc, char **argv)
{
    assert(argc == 3);
    const std::vector<uint8_t> validFixture = readFixture(argv[1]);
    const std::vector<uint8_t> invalidFixture = readFixture(argv[2]);
    PetBehaviorConfig config = {};
    assert(parseRuntimeTableBehavior(validFixture.data(), validFixture.size(),
                                     fixtureManifest(), 1, 1, config));
    assert(config.animationSceneCount == 3);
    PetBehaviorConfig unpublished = {};
    unpublished.animationSceneCount = 7;
    assert(!parseRuntimeTableBehavior(invalidFixture.data(), invalidFixture.size(),
                                      fixtureManifest(), 1, 1, unpublished));
    assert(unpublished.animationSceneCount == 7);

    const RuntimeAnimationSceneConfig &idleScene = config.animationScenes[0];
    const RuntimeAnimationSceneConfig &firstScene = config.animationScenes[1];
    const RuntimeAnimationSceneConfig &secondScene = config.animationScenes[2];
    Renderer renderer(nullptr, nullptr);
    Host host;
    CommandController commands(host);
    AnimationController animations(renderer);
    LayoutRenderer layout(renderer, commands);
    commands.configure(config);
    animations.configureRuntimeContract(config);
    layout.configureRuntimeContract(config);
    animations.setup(idleScene.animation);
    layout.begin();

    Animation queue[] = {
        Animation::complete(firstScene.animation),
        Animation::complete(secondScene.animation),
    };
    assert(animations.replace({queue, 2}) == PlaybackResult::Accepted);
    events.clear();
    prepareAndTick(animations, layout, 1000);
    assertAtomicSceneBeforeFrame(firstScene.layoutVersion, firstScene.animation.animationId);

    events.clear();
    prepareAndTick(animations, layout, 1001);
    assertAtomicSceneBeforeFrame(secondScene.layoutVersion, secondScene.animation.animationId);

    events.clear();
    prepareAndTick(animations, layout, 1002);
    assertAtomicSceneBeforeFrame(idleScene.layoutVersion, idleScene.animation.animationId);

    Animation first = Animation::complete(firstScene.animation);
    assert(animations.replace({&first, 1}) == PlaybackResult::Accepted);
    events.clear();
    prepareAndTick(animations, layout, 2000);
    assertAtomicSceneBeforeFrame(firstScene.layoutVersion, firstScene.animation.animationId);

    Animation interrupt = Animation::complete(secondScene.animation);
    assert(animations.replace({&interrupt, 1}) == PlaybackResult::Accepted);
    events.clear();
    prepareAndTick(animations, layout, 2001);
    assertAtomicSceneBeforeFrame(secondScene.layoutVersion, secondScene.animation.animationId);
    return 0;
}
