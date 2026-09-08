#include <stdio.h>
#include <fstream>
#include <iterator>
#include <vector>

#include "appearance/domain/RuntimeTableAppearance.h"
#include "pet_behavior/domain/RuntimeTableBehavior.h"
#include "pet_behavior/domain/PetBehaviorStatSlot.h"
#include "presentation/adapters/rendering/FrameDecoder.h"
#include "shared/assets/BundleReader.h"

namespace
{
uint16_t readU16(const std::vector<uint8_t> &bytes, size_t offset)
{
    return static_cast<uint16_t>(bytes[offset] | (static_cast<uint16_t>(bytes[offset + 1]) << 8));
}

uint32_t readU32(const std::vector<uint8_t> &bytes, size_t offset)
{
    return static_cast<uint32_t>(bytes[offset]) |
           (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}

bool decodePack(BundleReader &reader, const char *path, uint8_t speciesSlot)
{
    std::ifstream input(path, std::ios::binary);
    const std::vector<uint8_t> bytes{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (bytes.size() < AssetData::kHeaderSize)
        return false;

    const uint32_t animationCount = readU32(bytes, 36);
    const uint32_t animationOffset = readU32(bytes, 44);
    uint8_t readBuffer[FrameDecoder::kDataReadBufferBytes] = {};
    uint16_t lineBuffer[FrameDecoder::kLineBufferPixels] = {};
    Adafruit_ST7735 display;
    for (uint32_t index = 0; index < animationCount; ++index)
    {
        const size_t offset = animationOffset + index * AssetData::kAnimationRecordSize;
        const uint16_t animationId = readU16(bytes, offset);
        const uint8_t outfitSlot = bytes[offset + 2];
        const uint8_t versionIndex = bytes[offset + 3];
        const uint16_t frameCount = readU16(bytes, offset + 8);
        for (uint16_t frameIndex = 0; frameIndex < frameCount; ++frameIndex)
        {
            const AssetData::AssetFrameAddress address{
                speciesSlot, outfitSlot, animationId, versionIndex, frameIndex};
            if (!FrameDecoder::showDataFrame(
                    reader, address, &display, readBuffer, sizeof(readBuffer),
                    lineBuffer, FrameDecoder::kLineBufferPixels, 0, 32,
                    FrameDecoder::kWorkingBatchLines))
            {
                printf("frame decode failed: species=%u outfit=%u animation=%u version=%u frame=%u resource=%s\n",
                       speciesSlot, outfitSlot, animationId, versionIndex, frameIndex,
                       reader.firstErrorResource());
                return false;
            }
        }
    }
    return true;
}
}

int main(int argc, char **argv)
{
    if (argc != 2)
        return 64;

    SdFat sd(argv[1]);
    AssetData::RuntimeManifest manifest = {};
    if (!loadRuntimeManifest(&sd, manifest))
    {
        puts("runtime.bin envelope failed");
        return 1;
    }

    uint8_t scratch[AssetData::kIoScratchBytes] = {};
    BundleReader reader(&sd, scratch, sizeof(scratch));
    if (!reader.configureBundle(manifest.bundleId))
    {
        puts("bundle configuration failed");
        return 2;
    }

    PetBehaviorConfig config = {};
    if (!loadCompleteRuntimeTable(&sd, manifest, reader, 1, 1, config))
    {
        printf("runtime decode failed: %s\n", reader.firstErrorResource());
        return 3;
    }

    ActivePetBehaviorStatSlots slots(config);
    PetStatSnapshot stats = {};
    stats.speciesSlot = 1;
    stats.outfitSlot = 1;
    if (!validateRuntimeTableAppearance(&sd, manifest, reader, slots, stats))
    {
        printf("appearance validation failed: %s\n", reader.firstErrorResource());
        return 4;
    }

    AppearanceSelection initialAppearance = {};
    if (!loadRuntimeTableInitialAppearance(&sd, manifest, reader, initialAppearance))
    {
        printf("initial appearance failed: %s\n", reader.firstErrorResource());
        return 6;
    }
    stats.speciesSlot = initialAppearance.speciesSlot;
    stats.outfitSlot = initialAppearance.outfitSlot;
    uint8_t unlockMask = 0;
    if (!resolveRuntimeTableOutfitUnlockMask(
            &sd, manifest, reader, initialAppearance.speciesSlot, slots,
            stats, 0, true, unlockMask))
    {
        printf("initial unlock failed: species=%u outfit=%u resource=%s\n",
               initialAppearance.speciesSlot, initialAppearance.outfitSlot,
               reader.firstErrorResource());
        return 7;
    }
    if (initialAppearance.outfitSlot == 0 || initialAppearance.outfitSlot > 8 ||
        (unlockMask & (1U << (initialAppearance.outfitSlot - 1U))) == 0)
    {
        printf("initial outfit remains locked: species=%u outfit=%u mask=%u\n",
               initialAppearance.speciesSlot, initialAppearance.outfitSlot, unlockMask);
        return 8;
    }

    const std::filesystem::path root(argv[1]);
    if (!decodePack(reader, (root / "assets/shared.data").string().c_str(), 0) ||
        !decodePack(reader, (root / "assets/species_1.data").string().c_str(), 1))
        return 5;

    printf("valid: stats=%u actions=%u status=%u initial=%u/%u unlock=%u\n",
           config.statCount, config.actionCount, config.statusSets.count,
           initialAppearance.speciesSlot, initialAppearance.outfitSlot, unlockMask);
    return 0;
}
