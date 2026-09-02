#include "shared/assets/BundleReader.h"

#include <assert.h>
#include <string.h>
#include <vector>

namespace
{
void writeU16(std::vector<uint8_t> &bytes, size_t offset, uint16_t value)
{
    bytes[offset] = static_cast<uint8_t>(value);
    bytes[offset + 1] = static_cast<uint8_t>(value >> 8);
}

void writeU32(std::vector<uint8_t> &bytes, size_t offset, uint32_t value)
{
    for (size_t index = 0; index < 4; ++index)
        bytes[offset + index] = static_cast<uint8_t>(value >> (index * 8));
}

void writeAnimation(std::vector<uint8_t> &bytes,
                    size_t offset,
                    uint16_t animationId,
                    uint32_t firstFrame)
{
    writeU16(bytes, offset, animationId);
    bytes[offset + 2] = 0;
    bytes[offset + 3] = 0;
    writeU32(bytes, offset + 4, firstFrame);
    writeU16(bytes, offset + 8, 1);
    writeU16(bytes, offset + 10, 167);
}

std::vector<uint8_t> packWithInvalidUnselectedRows(const AssetData::BundleId &bundleId)
{
    constexpr uint32_t kAnimationOffset = 80;
    constexpr uint32_t kFrameOffset = kAnimationOffset + 3 * 16;
    constexpr uint32_t kPayloadOffset = kFrameOffset + 2 * 24;
    std::vector<uint8_t> bytes(kPayloadOffset + 2, 0);
    const uint8_t magic[8] = {'V', 'P', 'A', 'D', 'A', 'T', 'A', 0};
    memcpy(bytes.data(), magic, sizeof(magic));
    writeU16(bytes, 8, 1);
    writeU16(bytes, 10, 80);
    bytes[12] = AssetData::kPackKindShared;
    memcpy(bytes.data() + 16, bundleId.bytes, sizeof(bundleId.bytes));
    writeU32(bytes, 32, static_cast<uint32_t>(bytes.size()));
    writeU32(bytes, 36, 3);
    writeU32(bytes, 40, 2);
    writeU32(bytes, 44, kAnimationOffset);
    writeU32(bytes, 48, kFrameOffset);
    writeU32(bytes, 52, kPayloadOffset);
    writeU16(bytes, 56, 16);
    writeU16(bytes, 58, 24);
    writeU32(bytes, 64, 0xDEADBEEFUL); // Deliberately not the Pack CRC.

    writeAnimation(bytes, kAnimationOffset, 1, 1); // Selects invalid descriptor 1.
    writeAnimation(bytes, kAnimationOffset + 16, 2, 0); // Valid selected row.
    writeAnimation(bytes, kAnimationOffset + 32, 0, 0); // Invalid, never selected.

    bytes[kFrameOffset] = AssetData::kCodecRgb565RunLiteral;
    writeU16(bytes, kFrameOffset + 2, 32);
    writeU16(bytes, kFrameOffset + 4, 32);
    writeU32(bytes, kFrameOffset + 8, kPayloadOffset);
    writeU32(bytes, kFrameOffset + 12, 2);
    writeU32(bytes, kFrameOffset + 16, 32 * 32 * 2);
    bytes[kFrameOffset + 24] = 99; // Invalid, never selected by animation 2.
    bytes[kPayloadOffset] = 0x80;
    bytes[kPayloadOffset + 1] = 0;
    return bytes;
}
} // namespace

int main()
{
    AssetData::BundleId bundleId;
    for (size_t index = 0; index < sizeof(bundleId.bytes); ++index)
        bundleId.bytes[index] = static_cast<uint8_t>(index + 1);
    const std::vector<uint8_t> bytes = packWithInvalidUnselectedRows(bundleId);
    uint8_t scratch[AssetData::kVerificationScratchBytes] = {};
    SdFat sd(bytes.data(), bytes.size());

    BundleReader reader(&sd, scratch, sizeof(scratch));
    assert(reader.configureBundle(bundleId));
    AssetData::AssetFrameAddress validAddress{0, 0, 2, 0, 0};
    AssetData::OpenFrame frame;
    assert(reader.openFrame(validAddress, frame));
    assert(frame.descriptor.codec == AssetData::kCodecRgb565RunLiteral);
    frame.file.close();

    BundleReader selectedFailureReader(&sd, scratch, sizeof(scratch));
    assert(selectedFailureReader.configureBundle(bundleId));
    AssetData::AssetFrameAddress invalidSelectedDescriptor{0, 0, 1, 0, 0};
    assert(!selectedFailureReader.openFrame(invalidSelectedDescriptor, frame));
    assert(selectedFailureReader.firstError() == AssetData::BundleError::InvalidFrame);
    return 0;
}
