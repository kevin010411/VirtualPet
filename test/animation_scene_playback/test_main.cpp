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
enum class RenderEventChannel : uint8_t
{
    SdRead,
    TftDraw,
};

enum class RenderEventKind : uint8_t
{
    AnimationRecord,
    LayoutMetadata,
    LayoutFrame,
    AnimationFrame,
};

struct RenderEvent
{
    RenderEventChannel channel;
    RenderEventKind kind;
    uint16_t animationId;
    uint8_t version;
    uint16_t frame;
};

std::vector<RenderEvent> events;

void recordEvent(RenderEventChannel channel, RenderEventKind kind,
                 uint16_t animationId, uint8_t version, uint16_t frame)
{
    events.push_back({channel, kind, animationId, version, frame});
}
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
    assert(events.size() == 23);
    assert(events[0].channel == RenderEventChannel::SdRead);
    assert(events[0].kind == RenderEventKind::AnimationRecord);
    for (size_t index = 1; index <= 2; ++index)
    {
        assert(events[index].channel == RenderEventChannel::SdRead);
        assert(events[index].kind == RenderEventKind::LayoutMetadata);
        assert(events[index].version == layoutVersion);
    }
    for (size_t index = 0; index < 9; ++index)
    {
        const size_t readIndex = 3 + index * 2;
        const size_t drawIndex = readIndex + 1;
        assert(events[readIndex].channel == RenderEventChannel::SdRead);
        assert(events[readIndex].kind == RenderEventKind::LayoutFrame);
        assert(events[drawIndex].channel == RenderEventChannel::TftDraw);
        assert(events[drawIndex].kind == RenderEventKind::LayoutFrame);
        assert(events[drawIndex].version == layoutVersion);
        assert(events[drawIndex].frame == (index == 0 ? 1 : index + 2));
    }
    assert(events[21].channel == RenderEventChannel::SdRead);
    assert(events[21].kind == RenderEventKind::AnimationFrame);
    assert(events[22].channel == RenderEventChannel::TftDraw);
    assert(events[22].kind == RenderEventKind::AnimationFrame);
    assert(events[22].animationId == centerId);
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
    recordEvent(RenderEventChannel::SdRead, RenderEventKind::AnimationRecord,
                reference.animationId, versionIndex, 0);
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
                                     uint8_t layoutId, uint8_t)
{
    recordEvent(RenderEventChannel::SdRead, RenderEventKind::LayoutMetadata,
                unselected.animationId, layoutId, 0);
    recordEvent(RenderEventChannel::SdRead, RenderEventKind::LayoutMetadata,
                selected.animationId, layoutId, 0);
    return unselected.shared() && selected.shared();
}
AssetData::BundleError Renderer::firstAssetDataError() const
{
    return AssetData::BundleError::None;
}
bool Renderer::advanceAnimationFrame()
{
    recordEvent(RenderEventChannel::SdRead, RenderEventKind::AnimationFrame,
                selectedAnimationId, selectedVersion, 1);
    recordEvent(RenderEventChannel::TftDraw, RenderEventKind::AnimationFrame,
                selectedAnimationId, selectedVersion, 1);
    return true;
}
bool Renderer::ShowAnimationFrame(
    const AssetData::AnimationRef &reference, uint8_t versionIndex,
    uint16_t frameIndex, int, int, int, uint16_t, uint16_t)
{
    recordEvent(RenderEventChannel::SdRead, RenderEventKind::LayoutFrame,
                reference.animationId, versionIndex, frameIndex);
    recordEvent(RenderEventChannel::TftDraw, RenderEventKind::LayoutFrame,
                reference.animationId, versionIndex, frameIndex);
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
    config.buttons[1] = config.buttons[0];

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
    events.clear();
    assert(animations.replace({&first, 1}) == PlaybackResult::Accepted);
    prepareAndTick(animations, layout, renderer, 1000);
    assertAtomicSceneBeforeFrame(0, center.animationId);

    // A same-layout animation reads its record and frame without layout rereads/redraws.
    events.clear();
    assert(animations.replace({&first, 1}) == PlaybackResult::Accepted);
    prepareAndTick(animations, layout, renderer, 1001);
    assert(events.size() == 3);
    assert(events[0].channel == RenderEventChannel::SdRead);
    assert(events[0].kind == RenderEventKind::AnimationRecord);
    assert(events[1].channel == RenderEventChannel::SdRead);
    assert(events[1].kind == RenderEventKind::AnimationFrame);
    assert(events[2].channel == RenderEventChannel::TftDraw);
    assert(events[2].kind == RenderEventKind::AnimationFrame);

    Animation second = Animation::complete(center);
    second.versionIndex = 1;
    events.clear();
    assert(animations.replace({&second, 1}) == PlaybackResult::Accepted);
    prepareAndTick(animations, layout, renderer, 1002);
    assertAtomicSceneBeforeFrame(1, center.animationId);

    // A display wake redraws all layout frames; SD reads and TFT draws are distinct.
    events.clear();
    layout.drawAll();
    assert(events.size() == 18);
    for (size_t index = 0; index < 9; ++index)
    {
        const size_t readIndex = index * 2;
        const size_t drawIndex = readIndex + 1;
        assert(events[readIndex].channel == RenderEventChannel::SdRead);
        assert(events[readIndex].kind == RenderEventKind::LayoutFrame);
        assert(events[drawIndex].channel == RenderEventChannel::TftDraw);
        assert(events[drawIndex].kind == RenderEventKind::LayoutFrame);
        assert(events[drawIndex].frame == (index == 0 ? 1 : index + 2));
    }

    // Selection redraws only the previous and current visible buttons.
    events.clear();
    commands.next();
    layout.drawSelection();
    assert(events.size() == 4);
    assert(events[0].channel == RenderEventChannel::SdRead);
    assert(events[0].kind == RenderEventKind::LayoutFrame);
    assert(events[1].channel == RenderEventChannel::TftDraw);
    assert(events[1].kind == RenderEventKind::LayoutFrame);
    assert(events[1].frame == 3);
    assert(events[2].channel == RenderEventChannel::SdRead);
    assert(events[2].kind == RenderEventKind::LayoutFrame);
    assert(events[3].channel == RenderEventChannel::TftDraw);
    assert(events[3].kind == RenderEventKind::LayoutFrame);
    assert(events[3].frame == 4);

    // Appearance changes invalidate cached layout metadata before the next frame.
    events.clear();
    layout.begin();
    assert(layout.updatePlayback(1));
    assert(events.size() == 20);
    assert(events[0].channel == RenderEventChannel::SdRead);
    assert(events[0].kind == RenderEventKind::LayoutMetadata);
    assert(events[1].channel == RenderEventChannel::SdRead);
    assert(events[1].kind == RenderEventKind::LayoutMetadata);
    for (size_t index = 0; index < 9; ++index)
    {
        const size_t readIndex = 2 + index * 2;
        const size_t drawIndex = readIndex + 1;
        assert(events[readIndex].channel == RenderEventChannel::SdRead);
        assert(events[readIndex].kind == RenderEventKind::LayoutFrame);
        assert(events[drawIndex].channel == RenderEventChannel::TftDraw);
        assert(events[drawIndex].kind == RenderEventKind::LayoutFrame);
    }
    return 0;
}
