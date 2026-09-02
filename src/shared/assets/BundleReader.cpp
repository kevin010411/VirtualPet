#include "shared/assets/BundleReader.h"

#include "shared/sd/SdBinaryRead.h"

#include <string.h>
#include "shared/utils/TextBuffer.h"

namespace
{
constexpr uint8_t kMagic[8] = {'V', 'P', 'A', 'D', 'A', 'T', 'A', 0};

uint16_t readU16(const uint8_t *data)
{
    return static_cast<uint16_t>(data[0] | (static_cast<uint16_t>(data[1]) << 8));
}

uint32_t readU32(const uint8_t *data)
{
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

bool readExact(SdBaseFile &file, uint32_t offset, uint8_t *destination, size_t size)
{
    return destination != nullptr && file.seekSet(offset) &&
           readSdBinary(file, destination, size) == static_cast<int>(size);
}

int compareAnimationKey(const AssetData::AnimationRecord &left,
                        const AssetData::AnimationRecord &right)
{
    if (left.outfitSlot != right.outfitSlot)
        return left.outfitSlot < right.outfitSlot ? -1 : 1;
    if (left.animationId != right.animationId)
        return left.animationId < right.animationId ? -1 : 1;
    if (left.versionIndex != right.versionIndex)
        return left.versionIndex < right.versionIndex ? -1 : 1;
    return 0;
}

bool descriptorsMatch(const AssetData::FrameDescriptor &left,
                      const AssetData::FrameDescriptor &right)
{
    return left.codec == right.codec &&
           left.width == right.width &&
           left.height == right.height &&
           left.payloadOffset == right.payloadOffset &&
           left.encodedSize == right.encodedSize &&
           left.decodedSize == right.decodedSize;
}

} // namespace

struct BundleReader::PackHeader
{
    uint8_t packKind = 0;
    uint8_t speciesSlot = 0;
    uint8_t bundleId[16] = {};
    uint32_t fileSize = 0;
    uint32_t animationCount = 0;
    uint32_t frameCount = 0;
    uint32_t animationTableOffset = 0;
    uint32_t frameTableOffset = 0;
    uint32_t payloadOffset = 0;
    uint32_t crc32 = 0;
};

BundleReader::BundleReader(SdFat *sd, uint8_t *scratch, size_t scratchSize)
    : sd_(sd), scratch_(scratch), scratchSize_(scratchSize)
{
}

bool BundleReader::configureBundle(const AssetData::BundleId &bundleId)
{
    if (sd_ == nullptr || scratch_ == nullptr ||
        scratchSize_ < AssetData::kIoScratchBytes)
    {
        return recordError(AssetData::BundleError::NotConfigured, 0);
    }

    if (bundleConfigured_)
    {
        if (memcmp(bundleId_.bytes, bundleId.bytes, sizeof(bundleId_.bytes)) == 0)
            return firstError_ == AssetData::BundleError::None;
        return recordError(AssetData::BundleError::NotConfigured, 0);
    }

    bundleId_ = bundleId;
    bundleConfigured_ = true;
    return firstError_ == AssetData::BundleError::None;
}

bool BundleReader::resolveAnimation(const AssetData::AssetFrameAddress &address,
                                    AssetData::AnimationRecord &animation)
{
    animation = AssetData::AnimationRecord{};
    if (!validateAddress(address))
        return false;

    SdBaseFile file;
    PackHeader header;
    if (!openPack(address.speciesSlot, file, header))
        return false;

    const bool found = findAnimation(file, header, address, animation);
    file.close();
    if (!found)
        return recordError(AssetData::BundleError::MissingAnimation, address.speciesSlot);
    return true;
}

bool BundleReader::tryResolveAnimation(const AssetData::AssetFrameAddress &address,
                                       AssetData::AnimationRecord &animation)
{
    animation = AssetData::AnimationRecord{};
    if (!validateAddress(address))
        return false;

    SdBaseFile file;
    PackHeader header;
    if (!openPack(address.speciesSlot, file, header))
        return false;

    const bool found = findAnimation(file, header, address, animation);
    file.close();
    return found;
}

bool BundleReader::openFrame(const AssetData::AssetFrameAddress &address,
                             AssetData::OpenFrame &frame)
{
    frame.file.close();
    frame.descriptor = AssetData::FrameDescriptor{};
    frame.animationFrameCount = 0;
    frame.frameMs = 0;
    if (!validateAddress(address))
        return false;

    PackHeader header;
    if (!openPack(address.speciesSlot, frame.file, header))
        return false;

    AssetData::AnimationRecord animation;
    if (!findAnimation(frame.file, header, address, animation))
    {
        frame.file.close();
        return recordError(AssetData::BundleError::MissingAnimation, address.speciesSlot);
    }
    if (address.frameIndex >= animation.frameCount)
    {
        frame.file.close();
        return recordError(AssetData::BundleError::InvalidFrame, address.speciesSlot);
    }

    const uint32_t descriptorIndex = animation.firstFrame + address.frameIndex;
    if (!readDescriptor(frame.file, header, descriptorIndex, frame.descriptor) ||
        !frame.file.seekSet(frame.descriptor.payloadOffset))
    {
        frame.file.close();
        return recordError(AssetData::BundleError::InvalidFrame, address.speciesSlot);
    }

    frame.animationFrameCount = animation.frameCount;
    frame.frameMs = animation.frameMs;
    return true;
}

void BundleReader::rejectDecodedFrame(const AssetData::AssetFrameAddress &address)
{
    recordError(AssetData::BundleError::DecodeFailed, address.speciesSlot);
}

AssetData::BundleError BundleReader::firstError() const
{
    return firstError_;
}

const char *BundleReader::firstErrorResource() const
{
    return firstErrorResource_;
}

bool BundleReader::validateAddress(const AssetData::AssetFrameAddress &address)
{
    if (firstError_ != AssetData::BundleError::None)
        return false;
    if (!bundleConfigured_)
        return recordError(AssetData::BundleError::NotConfigured, address.speciesSlot);

    const bool sharedAddress = address.speciesSlot == 0 && address.outfitSlot == 0;
    const bool speciesAddress = address.speciesSlot != 0 && address.outfitSlot != 0;
    if ((!sharedAddress && !speciesAddress) ||
        address.animationId == 0 ||
        address.animationId > AssetData::kMaxRuntimeAnimationId ||
        address.versionIndex >= AssetData::kMaxVersions)
    {
        return recordError(AssetData::BundleError::InvalidAddress, address.speciesSlot);
    }
    return true;
}

bool BundleReader::buildPackPath(uint8_t speciesSlot, char *path, size_t pathSize) const
{
    if (path == nullptr || pathSize == 0)
        return false;

    TextBuffer output(path, pathSize);
    if (speciesSlot == 0)
        return output.append("/assets/shared.data") && output.ok();
    return output.append("/assets/species_") && output.appendUnsigned(speciesSlot) &&
           output.append(".data") && output.ok();
}

bool BundleReader::openPack(uint8_t speciesSlot, SdBaseFile &file, PackHeader &header)
{
    if (firstError_ != AssetData::BundleError::None)
        return false;

    char path[32];
    if (!buildPackPath(speciesSlot, path, sizeof(path)))
        return recordError(AssetData::BundleError::InvalidAddress, speciesSlot);

    if (!file.open(path, FILE_READ))
        return recordError(AssetData::BundleError::MissingPack, speciesSlot);

    const uint32_t physicalSize = file.fileSize();
    if (physicalSize < AssetData::kHeaderSize || physicalSize > AssetData::kMaxPackBytes ||
        !readHeader(file, header) || !validateHeader(header, speciesSlot, physicalSize))
    {
        file.close();
        return recordError(AssetData::BundleError::InvalidPack, speciesSlot);
    }
    return true;
}

bool BundleReader::readHeader(SdBaseFile &file, PackHeader &header)
{
    if (scratch_ == nullptr || scratchSize_ < AssetData::kHeaderSize ||
        !readExact(file, 0, scratch_, AssetData::kHeaderSize))
    {
        return false;
    }

    const uint8_t *data = scratch_;
    if (memcmp(data, kMagic, sizeof(kMagic)) != 0 ||
        readU16(data + 8) != AssetData::kVersion ||
        readU16(data + 10) != AssetData::kHeaderSize ||
        readU16(data + 14) != 0 ||
        readU16(data + 56) != AssetData::kAnimationRecordSize ||
        readU16(data + 58) != AssetData::kFrameRecordSize ||
        readU32(data + 60) != 0)
    {
        return false;
    }
    for (size_t index = 68; index < AssetData::kHeaderSize; ++index)
    {
        if (data[index] != 0)
            return false;
    }

    header.packKind = data[12];
    header.speciesSlot = data[13];
    memcpy(header.bundleId, data + 16, sizeof(header.bundleId));
    header.fileSize = readU32(data + 32);
    header.animationCount = readU32(data + 36);
    header.frameCount = readU32(data + 40);
    header.animationTableOffset = readU32(data + 44);
    header.frameTableOffset = readU32(data + 48);
    header.payloadOffset = readU32(data + 52);
    header.crc32 = readU32(data + 64);
    return true;
}

bool BundleReader::validateHeader(const PackHeader &header,
                                  uint8_t speciesSlot,
                                  uint32_t physicalSize) const
{
    const uint8_t expectedKind = speciesSlot == 0 ? AssetData::kPackKindShared : AssetData::kPackKindSpecies;
    if (physicalSize < AssetData::kHeaderSize || physicalSize > AssetData::kMaxPackBytes ||
        header.fileSize != physicalSize ||
        header.packKind != expectedKind ||
        header.speciesSlot != speciesSlot ||
        memcmp(header.bundleId, bundleId_.bytes, sizeof(bundleId_.bytes)) != 0 ||
        header.animationCount == 0 || header.animationCount > AssetData::kMaxAnimationRecords ||
        header.frameCount == 0 || header.frameCount > AssetData::kMaxFrameDescriptors)
    {
        return false;
    }

    const uint32_t expectedFrameOffset = AssetData::kHeaderSize +
                                         header.animationCount * AssetData::kAnimationRecordSize;
    const uint32_t expectedPayloadOffset = expectedFrameOffset +
                                           header.frameCount * AssetData::kFrameRecordSize;
    return header.animationTableOffset == AssetData::kHeaderSize &&
           header.frameTableOffset == expectedFrameOffset &&
           header.payloadOffset == expectedPayloadOffset &&
           header.payloadOffset < header.fileSize;
}

bool BundleReader::findAnimation(SdBaseFile &file,
                                 const PackHeader &header,
                                 const AssetData::AssetFrameAddress &address,
                                 AssetData::AnimationRecord &animation)
{
    AssetData::AnimationRecord target;
    target.outfitSlot = address.outfitSlot;
    target.animationId = address.animationId;
    target.versionIndex = address.versionIndex;

    uint32_t low = 0;
    uint32_t high = header.animationCount;
    while (low < high)
    {
        const uint32_t middle = low + (high - low) / 2;
        AssetData::AnimationRecord candidate;
        if (!readAnimation(file, header, middle, candidate))
            return false;
        const int comparison = compareAnimationKey(candidate, target);
        if (comparison < 0)
            low = middle + 1;
        else
            high = middle;
    }

    if (low >= header.animationCount || !readAnimation(file, header, low, animation))
        return false;
    return compareAnimationKey(animation, target) == 0;
}

bool BundleReader::readAnimation(SdBaseFile &file,
                                 const PackHeader &header,
                                 uint32_t index,
                                 AssetData::AnimationRecord &animation) const
{
    if (index >= header.animationCount)
        return false;
    uint8_t data[AssetData::kAnimationRecordSize];
    const uint32_t offset = header.animationTableOffset + index * AssetData::kAnimationRecordSize;
    if (!readExact(file, offset, data, sizeof(data)) || readU32(data + 12) != 0)
        return false;

    animation.animationId = readU16(data);
    animation.outfitSlot = data[2];
    animation.versionIndex = data[3];
    animation.firstFrame = readU32(data + 4);
    animation.frameCount = readU16(data + 8);
    animation.frameMs = readU16(data + 10);
    const bool validOutfit =
        (header.packKind == AssetData::kPackKindShared && animation.outfitSlot == 0) ||
        (header.packKind == AssetData::kPackKindSpecies && animation.outfitSlot > 0);
    return animation.animationId > 0 &&
           animation.animationId <= AssetData::kMaxRuntimeAnimationId &&
           validOutfit &&
           animation.versionIndex < AssetData::kMaxVersions &&
           animation.frameCount > 0 &&
           animation.frameCount <= AssetData::kMaxFramesPerAnimation &&
           animation.frameMs > 0 && animation.frameMs <= 60000 &&
           animation.firstFrame <= header.frameCount &&
           animation.frameCount <= header.frameCount - animation.firstFrame;
}

bool BundleReader::readDescriptor(SdBaseFile &file,
                                  const PackHeader &header,
                                  uint32_t index,
                                  AssetData::FrameDescriptor &descriptor) const
{
    if (index >= header.frameCount)
        return false;
    uint8_t data[AssetData::kFrameRecordSize];
    const uint32_t offset = header.frameTableOffset + index * AssetData::kFrameRecordSize;
    if (!readExact(file, offset, data, sizeof(data)) || data[1] != 0 ||
        readU16(data + 6) != 0 || readU32(data + 20) != 0)
    {
        return false;
    }

    descriptor.codec = data[0];
    descriptor.width = readU16(data + 2);
    descriptor.height = readU16(data + 4);
    descriptor.payloadOffset = readU32(data + 8);
    descriptor.encodedSize = readU32(data + 12);
    descriptor.decodedSize = readU32(data + 16);
    const bool validDimensions = (descriptor.width == 128 && descriptor.height == 96) ||
                                 (descriptor.width == 32 && descriptor.height == 32);
    const uint32_t expectedDecodedSize = static_cast<uint32_t>(descriptor.width) *
                                         static_cast<uint32_t>(descriptor.height) * 2UL;
    return validDimensions &&
           (descriptor.codec == AssetData::kCodecPal8RunLiteral ||
            descriptor.codec == AssetData::kCodecRgb565RunLiteral) &&
           expectedDecodedSize <= AssetData::kMaxDecodedBytes &&
           descriptor.decodedSize == expectedDecodedSize &&
           descriptor.encodedSize > 0 &&
           descriptor.payloadOffset >= header.payloadOffset &&
           descriptor.payloadOffset <= header.fileSize &&
           descriptor.encodedSize <= header.fileSize - descriptor.payloadOffset;
}

bool BundleReader::recordError(AssetData::BundleError error, uint8_t speciesSlot)
{
    if (firstError_ != AssetData::BundleError::None)
        return false;

    firstError_ = error;
    const char *resource = nullptr;
    if (error == AssetData::BundleError::NotConfigured)
        resource = "asset bundle";
    else if (error == AssetData::BundleError::InvalidAddress)
        resource = "asset reference";

    if (resource != nullptr)
    {
        strncpy(firstErrorResource_, resource, sizeof(firstErrorResource_) - 1);
        firstErrorResource_[sizeof(firstErrorResource_) - 1] = '\0';
        return false;
    }

    char path[32];
    if (!buildPackPath(speciesSlot, path, sizeof(path)))
    {
        strncpy(firstErrorResource_, "asset data", sizeof(firstErrorResource_) - 1);
        firstErrorResource_[sizeof(firstErrorResource_) - 1] = '\0';
        return false;
    }
    const char *name = strrchr(path, '/');
    name = name == nullptr ? path : name + 1;
    strncpy(firstErrorResource_, name, sizeof(firstErrorResource_) - 1);
    firstErrorResource_[sizeof(firstErrorResource_) - 1] = '\0';
    return false;
}
