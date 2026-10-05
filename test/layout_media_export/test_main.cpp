#include "resources/BundleReader.h"
#include "display/FrameDecoder.h"

#include <assert.h>
#include <fstream>
#include <iterator>
#include <string>
#include <string.h>
#include <vector>

namespace
{
std::vector<uint8_t> readFile(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    assert(file.good());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void verifyFixture(const std::string &root)
{
    std::ifstream records(root + "/frames.tsv");
    assert(records.good());
    std::string packPath, expectedPath;
    unsigned species, outfit, animation, version, frameIndex, width, height;
    int x, y;
    unsigned checked = 0;
    bool checkedAnimation = false, checkedButton = false;
    while (records >> packPath >> expectedPath >> species >> outfit >> animation >> version >> frameIndex >> width >> height >> x >> y)
    {
        const auto pack = readFile(root + "/" + packPath);
        const auto expected = readFile(root + "/" + expectedPath);
        assert(pack.size() >= AssetData::kHeaderSize);
        AssetData::BundleId bundleId;
        memcpy(bundleId.bytes, pack.data() + 16, sizeof(bundleId.bytes));
        SdFat sd(pack.data(), pack.size());
        uint8_t scratch[AssetData::kIoScratchBytes] = {};
        BundleReader reader(&sd, scratch, sizeof(scratch));
        assert(reader.configureBundle(bundleId));
        AssetData::AssetFrameAddress address{
            static_cast<uint8_t>(species), static_cast<uint8_t>(outfit),
            static_cast<uint16_t>(animation), static_cast<uint8_t>(version),
            static_cast<uint16_t>(frameIndex)};
        uint8_t readBuffer[FrameDecoder::kDataReadBufferBytes] = {};
        uint16_t lineBuffer[FrameDecoder::kLineBufferPixels] = {};
        Adafruit_ST7735 display;
        const bool button = width == 32 && height == 32;
        assert(width <= 128 && height <= 160 && width % 16 == 0 && height % 16 == 0);
        assert(FrameDecoder::showDataFrame(reader, address, &display,
            readBuffer, sizeof(readBuffer), lineBuffer, FrameDecoder::kLineBufferPixels,
            x, y, FrameDecoder::kWorkingBatchLines));
        assert(display.x == x && display.y == y);
        assert(display.windowWidth == static_cast<int>(width));
        assert(display.windowHeight == static_cast<int>(height));
        assert(display.pixels.size() == width * height);
        assert(expected.size() == display.pixels.size() * 2);
        for (size_t index = 0; index < display.pixels.size(); ++index)
            assert(display.pixels[index] == static_cast<uint16_t>(expected[index * 2] | (expected[index * 2 + 1] << 8)));
        checkedAnimation |= !button;
        checkedButton |= button;
        ++checked;
    }
    assert(records.eof());
    assert(checked > 0 && checkedAnimation && checkedButton);
}
} // namespace

int main(int argc, char **argv)
{
    assert(argc == 2);
    for (const char *name : {"contain-animation_area", "contain-full_device", "crop-animation_area", "crop-full_device"})
        verifyFixture(std::string(argv[1]) + "/" + name);
    return 0;
}
