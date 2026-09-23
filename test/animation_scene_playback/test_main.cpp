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
uint8_t selectedLayoutId = 0;
uint8_t fixtureLayoutIds[2] = {};

AssetData::AnimationRef animation(uint16_t id) { return {1, 1, id}; }

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
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x46, 0x77,
        0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    for (uint8_t index = 0; index < sizeof(bundleId); ++index)
        manifest.bundleId.bytes[index] = bundleId[index];
    manifest.fileSize = static_cast<uint32_t>(fixture.size());
    manifest.schemaFingerprint = readFixtureU32(fixture, 44);
    manifest.fileCrc32 = readFixtureU32(fixture, 48);
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
                    Renderer &renderer,
                    unsigned long now)
{
    animations.preparePlayback(now);
    uint8_t layoutId = 0;
    assert(renderer.currentLayoutId(layoutId));
    assert(layout.updatePlayback(layoutId));
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
    selectedLayoutId = fixtureLayoutIds[versionIndex < 2 ? versionIndex : 0];
    return true;
}
bool Renderer::currentLayoutId(uint8_t &layoutId) const
{
    layoutId = selectedLayoutId;
    return true;
}
bool Renderer::validateLayoutVersion(const AssetData::AnimationRef &unselected,
                                     const AssetData::AnimationRef &selected,
                                     uint8_t)
{
    return unselected.shared() && selected.shared();
}
AssetData::BundleError Renderer::firstAssetDataError() const
{
    return AssetData::BundleError::None;
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
    const std::vector<uint8_t> runtimeFixture = readFixture(argv[1]);
    const std::vector<uint8_t> packFixture = readFixture(argv[2]);
    PetBehaviorConfig config = {};
    assert(parseRuntimeTableBehavior(runtimeFixture.data(), runtimeFixture.size(),
                                     fixtureManifest(runtimeFixture), 1, 1, config));
    assert(config.layoutUnselected.shared() && config.layoutUnselected.valid());
    assert(config.layoutSelected.shared() && config.layoutSelected.valid());
    assert(packFixture.size() >= 112);
    assert(packFixture[12] == AssetData::kPackKindSpecies);
    fixtureLayoutIds[0] = packFixture[92];
    fixtureLayoutIds[1] = packFixture[108];
    assert(fixtureLayoutIds[0] == 0 && fixtureLayoutIds[1] == 1);

    Renderer renderer(nullptr, nullptr);
    Host host;
    CommandController commands(host);
    AnimationController animations(renderer);
    LayoutRenderer layout(renderer, commands);
    commands.configure(config);
    animations.configureRuntimeContract(config);
    layout.configureRuntimeContract(config);
    const AssetData::AnimationRef center = animation(1);
    animations.setup(center);
    layout.begin();

    Animation first = Animation::complete(center);
    first.versionIndex = 0;
    assert(animations.replace({&first, 1}) == PlaybackResult::Accepted);
    events.clear();
    prepareAndTick(animations, layout, renderer, 1000);
    assertAtomicSceneBeforeFrame(0, center.animationId);

    // Re-selecting an animation with the same layout keeps the button pixels.
    assert(animations.replace({&first, 1}) == PlaybackResult::Accepted);
    events.clear();
    prepareAndTick(animations, layout, renderer, 1001);
    assert(events.size() == 1);
    assert(events[0].kind == RenderEventKind::AnimationFrame);

    Animation second = Animation::complete(center);
    second.versionIndex = 1;
    assert(animations.replace({&second, 1}) == PlaybackResult::Accepted);
    events.clear();
    prepareAndTick(animations, layout, renderer, 1002);
    assertAtomicSceneBeforeFrame(1, center.animationId);

    // A display wake invalidates the pixels even when the layout ID is stable.
    events.clear();
    layout.drawAll();
    assert(events.size() == 8);
    return 0;
}
