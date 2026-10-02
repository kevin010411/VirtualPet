#include <assert.h>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "pet_behavior/domain/RuntimeTableBehavior.h"
#include "presentation/adapters/rendering/Renderer.h"
#include "presentation/application/LayoutRenderer.h"
#include "commands/application/CommandController.h"
#include "pet/domain/Pet.h"

namespace
{
struct Frame
{
    unsigned species, outfit, animation, version, index, width, height;
    int x, y;
    std::vector<uint16_t> pixels;
};

std::vector<Frame> readFrames(const std::string &root)
{
    std::ifstream input(root + "/frames.tsv");
    assert(input);
    std::vector<Frame> frames;
    std::string pack, expected;
    Frame frame;
    while (input >> pack >> expected >> frame.species >> frame.outfit >> frame.animation >>
           frame.version >> frame.index >> frame.width >> frame.height >> frame.x >> frame.y)
    {
        std::ifstream pixels(root + "/" + expected, std::ios::binary);
        assert(pixels);
        const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(pixels), std::istreambuf_iterator<char>()};
        assert(bytes.size() == frame.width * frame.height * 2);
        frame.pixels.clear();
        for (size_t index = 0; index < bytes.size(); index += 2)
            frame.pixels.push_back(bytes[index] | (bytes[index + 1] << 8));
        frames.push_back(frame);
    }
    assert(input.eof() && !frames.empty());
    return frames;
}

const Frame &findFrame(const std::vector<Frame> &frames, const AssetData::AnimationRef &ref,
                       uint8_t version, unsigned index)
{
    for (const Frame &frame : frames)
        if (frame.species == ref.speciesSlot && frame.outfit == ref.outfitSlot &&
            frame.animation == ref.animationId && frame.version == version && frame.index == index)
            return frame;
    assert(false);
    return frames[0];
}

void paint(std::vector<uint16_t> &screen, const Frame &frame)
{
    for (unsigned y = 0; y < frame.height; ++y)
        for (unsigned x = 0; x < frame.width; ++x)
            screen[(frame.y + y) * 128 + frame.x + x] = frame.pixels[y * frame.width + x];
}

class Host : public CommandHost
{
public:
    bool commandHasAnimation(FirmwarePlaybackRole) const override { return true; }
    bool commandCanStatus() const override { return true; }
    void commandStatus() override {}
    void commandChangeOutfit() override {}
    void commandPredict() override {}
    void commandGuessGame() override {}
};

void verify(const std::string &root)
{
    CustomLayoutHost::root = root;
    const auto frames = readFrames(root);
    SdFat sd;
    uint8_t scratch[AssetData::kIoScratchBytes] = {};
    BundleReader reader(&sd, scratch, sizeof(scratch));
    AssetData::RuntimeManifest manifest;
    assert(loadRuntimeManifest(&sd, manifest));
    assert(reader.configureBundle(manifest.bundleId));
    PetBehaviorConfig config = {};
    assert(loadCompleteRuntimeTable(&sd, manifest, reader, 1, 1, config));
    Adafruit_ST7735 display;
    Renderer renderer(&display, &sd);
    assert(renderer.configureAssetBundle(manifest.bundleId));
    // Consume every real exported frame, including both selection products and outfit previews.
    for (const Frame &frame : frames)
    {
        AssetData::AssetFrameAddress address{
            static_cast<uint8_t>(frame.species), static_cast<uint8_t>(frame.outfit),
            static_cast<uint16_t>(frame.animation), static_cast<uint8_t>(frame.version),
            static_cast<uint16_t>(frame.index)};
        assert(renderer.ShowDataFrame(address, frame.x, frame.y, 12, frame.width, frame.height));
        const auto &window = display.windows.back();
        assert(window.x == frame.x && window.y == frame.y &&
               window.width == static_cast<int>(frame.width) && window.height == static_cast<int>(frame.height));
        for (unsigned y = 0; y < frame.height; ++y)
            for (unsigned x = 0; x < frame.width; ++x)
                assert(display.screen[(frame.y + y) * 128 + frame.x + x] == frame.pixels[y * frame.width + x]);
    }
    renderer.setAnimationArea(0, 0, 0, 0);
    const ScreenBlockConfig *animationBlock = nullptr;
    for (uint8_t index = 0; index < config.screenBlockCount; ++index)
        if (config.screenBlocks[index].kind == ScreenBlockKind::Animation)
        {
            animationBlock = &config.screenBlocks[index];
            renderer.setAnimationArea(animationBlock->x, animationBlock->y,
                                       animationBlock->width, animationBlock->height);
        }
    Host host;
    CommandController commands(host);
    commands.configure(config);
    commands.resetSelection();
    LayoutRenderer layout(renderer, commands);
    layout.configureRuntimeContract(config);
    PetStatSnapshot snapshot = {};
    layout.updateValues(snapshot);
    const AssetData::AnimationRef active = config.idleAnimation;
    assert(renderer.setAnimation(active, 0, true));
    uint8_t layoutId;
    assert(renderer.currentLayoutId(layoutId));
    assert(layout.updatePlayback(layoutId));
    const auto expectedScreen = [&]() {
        std::vector<uint16_t> expected(128 * 160);
        paint(expected, findFrame(frames, config.layoutUnselected, layoutId, 0));
        for (uint8_t index = 0; index < config.screenBlockCount; ++index)
        {
            const auto &block = config.screenBlocks[index];
            if (block.kind == ScreenBlockKind::Stat)
            {
                uint16_t frame = block.fallbackFrame;
                if (block.source != kUnboundScreenSource)
                {
                    const int32_t value = block.source == kRuntimeValueStageDays
                        ? snapshot.stage_days : snapshot.customStats[runtimePetStatSlot(block.source)];
                    for (uint8_t child = 0; child < block.ruleCount; ++child)
                    {
                        const auto &rule = config.screenRules[block.firstRule + child];
                        if (value >= rule.minimum && value <= rule.maximum)
                        {
                            frame = rule.frame;
                            break;
                        }
                    }
                }
                paint(expected, findFrame(frames, config.layoutUnselected, layoutId, frame - 1));
                continue;
            }
            if (block.kind != ScreenBlockKind::Button) continue;
            const bool selected = commands.isSlotVisible(block.source - 1) &&
                                  commands.selectedSlot() == block.source - 1;
            paint(expected, findFrame(frames, selected ? config.layoutSelected : config.layoutUnselected,
                                      layoutId, index + 1));
        }
        if (animationBlock) paint(expected, findFrame(frames, active, 0, 0));
        return expected;
    };
    assert(renderer.ShowAnimationFrame(active, 0, 1));
    assert(display.screen == expectedScreen());
    if (config.screenRuleCount)
    {
        // Both inclusive ends, crossing ranges, null-image fallback, and blank restoration.
        for (int value : {9, 10, 20, 21, 30, 40, 41, 50, 60, 70, 80, 81})
        {
            snapshot.stage_days = value;
            for (auto &stat : snapshot.customStats) stat = value;
            display.windows.clear();
            layout.updateValues(snapshot);
            assert(display.screen == expectedScreen());
            for (const auto &window : display.windows)
            {
                bool numeric = false;
                for (uint8_t index = 0; index < config.screenBlockCount; ++index)
                {
                    const auto &block = config.screenBlocks[index];
                    numeric |= block.kind == ScreenBlockKind::Stat && block.x == window.x &&
                               block.y <= window.y && window.y < block.y + block.height;
                }
                assert(numeric);  // Animation and button pixels must survive value changes.
            }
            display.windows.clear();
            layout.updateValues(snapshot);
            assert(display.windows.empty());
        }
        // Distinct intervals mapped to the same image must also skip TFT writes.
        for (auto &stat : snapshot.customStats) stat = 10;
        snapshot.stage_days = 10;
        layout.updateValues(snapshot);
        for (auto &stat : snapshot.customStats) stat = 50;
        snapshot.stage_days = 50;
        display.windows.clear();
        layout.updateValues(snapshot);
        assert(display.windows.empty());
        // Different source values catch accidental Stage Days/Pet Stat aliasing.
        snapshot.stage_days = 30;
        for (auto &stat : snapshot.customStats) stat = 10;
        layout.updateValues(snapshot);
        assert(display.screen == expectedScreen());
        // Reconfiguring an appearance must display restored/current values, not Initial.
        layout.configureRuntimeContract(config);
        layout.updateValues(snapshot);
        assert(layout.updatePlayback(layoutId));
        assert(renderer.ShowAnimationFrame(active, 0, 1));
        assert(display.screen == expectedScreen());
    }
    for (int step = 0; step < 8; ++step)
    {
        display.windows.clear();
        commands.next();
        layout.drawSelection();
        assert(display.screen == expectedScreen());
        for (const auto &window : display.windows)
        {
            bool necessary = false;
            for (uint8_t index = 0; index < config.screenBlockCount; ++index)
            {
                const auto &block = config.screenBlocks[index];
                necessary |= block.kind == ScreenBlockKind::Button && window.x == block.x && window.y == block.y &&
                    (block.source == commands.selectedSlot() + 1 || block.source == commands.previousSlot() + 1);
            }
            assert(necessary);
        }
    }
    bool visible = false;
    for (int slot = 0; slot < 8; ++slot) visible |= commands.isSlotVisible(slot);
    assert(commands.executeCurrent() == visible);
    // Complete display invalidation must restore holes, transparent on images and the active frame.
    std::fill(display.screen.begin(), display.screen.end(), 0xdead);
    layout.begin();
    assert(layout.updatePlayback(layoutId));
    assert(renderer.ShowAnimationFrame(active, 0, 1));
    assert(display.screen == expectedScreen());
    assert(renderer.firstAssetDataError() == AssetData::BundleError::None);
}
}

int main(int argc, char **argv)
{
    assert(argc == 2 || argc == 3);
    if (argc == 3)
    {
        assert(std::string(argv[2]) == "--numeric");
        verify(argv[1]);
        return 0;
    }
    for (const char *name : {"moved", "enlarged", "shrunk-duplicates", "animation-only", "buttons-only", "empty"})
        verify(std::string(argv[1]) + "/" + name);
}
