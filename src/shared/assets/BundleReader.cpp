#include "shared/assets/BundleReader.h"

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

bool readExact(File &file, uint32_t offset, uint8_t *destination, size_t size)
{
    return destination != nullptr && file.seek(offset) &&
           file.read(destination, size) == static_cast<int>(size);
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
        scratchSize_ < AssetData::kVerificationScratchBytes)
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

    File file;
    PackHeader header;
    if (!openVerifiedPack(address.speciesSlot, file, header))
        return false;

    const bool found = findAnimation(file, header, address, animation);
    file.close();
    if (!found)
        return recordError(AssetData::BundleError::MissingAnimation, address.speciesSlot);
    return true;
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
    if (!openVerifiedPack(address.speciesSlot, frame.file, header))
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
        !frame.file.seek(frame.descriptor.payloadOffset))
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
    setVerificationState(address.speciesSlot, VerificationState::Failed);
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

bool BundleReader::openVerifiedPack(uint8_t speciesSlot, File &file, PackHeader &header)
{
    if (firstError_ != AssetData::BundleError::None)
        return false;

    const VerificationState state = verificationState(speciesSlot);
    if (state == VerificationState::Failed)
        return false;

    char path[32];
    if (!buildPackPath(speciesSlot, path, sizeof(path)))
        return recordError(AssetData::BundleError::InvalidAddress, speciesSlot);

    file = sd_->open(path);
    if (!file)
    {
        setVerificationState(speciesSlot, VerificationState::Failed);
        return recordError(AssetData::BundleError::MissingPack, speciesSlot);
    }

    if (state == VerificationState::Verified)
    {
        const uint32_t physicalSize = file.size();
        if (readHeader(file, header) &&
            validateHeader(header, speciesSlot, physicalSize) &&
            header.crc32 == verifiedCrc_[speciesSlot])
            return true;
        file.close();
        setVerificationState(speciesSlot, VerificationState::Failed);
        return recordError(AssetData::BundleError::InvalidPack, speciesSlot);
    }

    if (!verifyPack(file, speciesSlot, header))
    {
        file.close();
        setVerificationState(speciesSlot, VerificationState::Failed);
        return recordError(AssetData::BundleError::InvalidPack, speciesSlot);
    }

    verifiedCrc_[speciesSlot] = header.crc32;
    setVerificationState(speciesSlot, VerificationState::Verified);
    return true;
}

bool BundleReader::verifyPack(File &file, uint8_t speciesSlot, PackHeader &header)
{
    const uint32_t physicalSize = file.size();
    if (physicalSize < AssetData::kHeaderSize || physicalSize > AssetData::kMaxPackBytes ||
        !readHeader(file, header))
    {
        return false;
    }

    if (!validateHeader(header, speciesSlot, physicalSize))
        return false;

    return validateAnimations(file, header) &&
           validateDescriptors(file, header) &&
           validateCrc(file, header);
}

bool BundleReader::readHeader(File &file, PackHeader &header)
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

bool BundleReader::validateAnimations(File &file, const PackHeader &header)
{
    AssetData::AnimationRecord previous;
    bool hasPrevious = false;
    for (uint32_t index = 0; index < header.animationCount; ++index)
    {
        AssetData::AnimationRecord current;
        if (!readAnimation(file, header, index, current) ||
            current.animationId == 0 || current.animationId > AssetData::kMaxRuntimeAnimationId ||
            current.versionIndex >= AssetData::kMaxVersions ||
            current.frameCount == 0 || current.frameCount > AssetData::kMaxFramesPerAnimation ||
            current.frameMs == 0 || current.frameMs > 60000 ||
            current.firstFrame > header.frameCount ||
            current.frameCount > header.frameCount - current.firstFrame)
        {
            return false;
        }

        if ((header.packKind == AssetData::kPackKindShared && current.outfitSlot != 0) ||
            (header.packKind == AssetData::kPackKindSpecies && current.outfitSlot == 0))
        {
            return false;
        }

        if (hasPrevious)
        {
            if (compareAnimationKey(previous, current) >= 0)
                return false;
            const bool sameAnimation = previous.outfitSlot == current.outfitSlot &&
                                       previous.animationId == current.animationId;
            if ((sameAnimation && current.versionIndex != previous.versionIndex + 1) ||
                (!sameAnimation && current.versionIndex != 0))
            {
                return false;
            }
            if (header.packKind == AssetData::kPackKindSpecies &&
                current.outfitSlot != previous.outfitSlot &&
                current.outfitSlot != static_cast<uint8_t>(previous.outfitSlot + 1))
            {
                return false;
            }
        }
        else if (current.versionIndex != 0 ||
                 (header.packKind == AssetData::kPackKindSpecies && current.outfitSlot != 1))
        {
            return false;
        }

        previous = current;
        hasPrevious = true;
    }
    return validateAnimationRanges(file, header);
}

bool BundleReader::validateAnimationRanges(File &file, const PackHeader &header)
{
    constexpr uint32_t kFramesPerPass = AssetData::kVerificationScratchBytes * 8UL;
    for (uint32_t blockStart = 0; blockStart < header.frameCount; blockStart += kFramesPerPass)
    {
        memset(scratch_, 0, AssetData::kVerificationScratchBytes);
        uint32_t blockEnd = blockStart + kFramesPerPass;
        if (blockEnd > header.frameCount)
            blockEnd = header.frameCount;

        for (uint32_t index = 0; index < header.animationCount; ++index)
        {
            AssetData::AnimationRecord current;
            if (!readAnimation(file, header, index, current))
                return false;
            const uint32_t rangeStart = current.firstFrame;
            const uint32_t rangeEnd = current.firstFrame + current.frameCount;
            const uint32_t overlapStart = rangeStart > blockStart ? rangeStart : blockStart;
            const uint32_t overlapEnd = rangeEnd < blockEnd ? rangeEnd : blockEnd;
            if (overlapStart >= overlapEnd)
                continue;

            bool collision = false;
            for (uint32_t frameIndex = overlapStart; frameIndex < overlapEnd; ++frameIndex)
            {
                const uint32_t bitIndex = frameIndex - blockStart;
                if ((scratch_[bitIndex / 8] & static_cast<uint8_t>(1U << (bitIndex % 8))) != 0)
                {
                    collision = true;
                    break;
                }
            }

            if (collision)
            {
                bool exactPriorRange = false;
                for (uint32_t priorIndex = index; priorIndex > 0; --priorIndex)
                {
                    AssetData::AnimationRecord prior;
                    if (!readAnimation(file, header, priorIndex - 1, prior))
                        return false;
                    if (prior.firstFrame == current.firstFrame && prior.frameCount == current.frameCount)
                    {
                        exactPriorRange = true;
                        break;
                    }
                }
                if (!exactPriorRange)
                    return false;
                continue;
            }

            for (uint32_t frameIndex = overlapStart; frameIndex < overlapEnd; ++frameIndex)
            {
                const uint32_t bitIndex = frameIndex - blockStart;
                scratch_[bitIndex / 8] |= static_cast<uint8_t>(1U << (bitIndex % 8));
            }
        }
    }
    return true;
}

bool BundleReader::validateDescriptors(File &file, const PackHeader &header)
{
    uint32_t payloadCursor = header.payloadOffset;
    for (uint32_t index = 0; index < header.frameCount; ++index)
    {
        AssetData::FrameDescriptor current;
        if (!readDescriptor(file, header, index, current))
            return false;

        const bool validDimensions = (current.width == 128 && current.height == 96) ||
                                     (current.width == 32 && current.height == 32);
        if (!validDimensions)
            return false;
        const uint32_t expectedDecodedSize = static_cast<uint32_t>(current.width) *
                                             static_cast<uint32_t>(current.height) * 2UL;
        if ((current.codec != AssetData::kCodecPal8RunLiteral &&
             current.codec != AssetData::kCodecRgb565RunLiteral) ||
            expectedDecodedSize > AssetData::kMaxDecodedBytes ||
            current.decodedSize != expectedDecodedSize || current.encodedSize == 0 ||
            current.payloadOffset < header.payloadOffset ||
            current.payloadOffset > header.fileSize ||
            current.encodedSize > header.fileSize - current.payloadOffset)
        {
            return false;
        }

        if (current.payloadOffset == payloadCursor)
        {
            payloadCursor += current.encodedSize;
            continue;
        }
        if (current.payloadOffset > payloadCursor)
            return false;

        bool exactEarlierPayload = false;
        for (uint32_t priorIndex = index; priorIndex > 0; --priorIndex)
        {
            AssetData::FrameDescriptor prior;
            if (!readDescriptor(file, header, priorIndex - 1, prior))
                return false;
            if (prior.payloadOffset != current.payloadOffset)
                continue;
            exactEarlierPayload = descriptorsMatch(prior, current);
            break;
        }
        if (!exactEarlierPayload)
            return false;
    }
    return payloadCursor == header.fileSize;
}

bool BundleReader::validateCrc(File &file, const PackHeader &header)
{
    if (!file.seek(0))
        return false;

    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t offset = 0;
    while (offset < header.fileSize)
    {
        size_t chunkSize = scratchSize_;
        if (chunkSize > AssetData::kVerificationScratchBytes)
            chunkSize = AssetData::kVerificationScratchBytes;
        const uint32_t remaining = header.fileSize - offset;
        if (chunkSize > remaining)
            chunkSize = static_cast<size_t>(remaining);
        if (file.read(scratch_, chunkSize) != static_cast<int>(chunkSize))
            return false;

        for (size_t index = 0; index < chunkSize; ++index)
        {
            const uint32_t absoluteOffset = offset + static_cast<uint32_t>(index);
            const uint8_t byte = absoluteOffset >= 64 && absoluteOffset <= 67
                                     ? 0
                                     : scratch_[index];
            crc ^= byte;
            for (uint8_t bit = 0; bit < 8; ++bit)
                crc = (crc & 1U) != 0 ? (crc >> 1) ^ 0xEDB88320UL : crc >> 1;
        }
        offset += static_cast<uint32_t>(chunkSize);
    }
    return (crc ^ 0xFFFFFFFFUL) == header.crc32;
}

bool BundleReader::findAnimation(File &file,
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

bool BundleReader::readAnimation(File &file,
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
    return true;
}

bool BundleReader::readDescriptor(File &file,
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
    return true;
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

BundleReader::VerificationState BundleReader::verificationState(uint8_t speciesSlot) const
{
    const uint8_t byte = verificationStates_[speciesSlot / 4];
    const uint8_t shift = static_cast<uint8_t>((speciesSlot % 4) * 2);
    return static_cast<VerificationState>((byte >> shift) & 0x03U);
}

void BundleReader::setVerificationState(uint8_t speciesSlot, VerificationState state)
{
    uint8_t &byte = verificationStates_[speciesSlot / 4];
    const uint8_t shift = static_cast<uint8_t>((speciesSlot % 4) * 2);
    byte = static_cast<uint8_t>((byte & ~(0x03U << shift)) |
                                (static_cast<uint8_t>(state) << shift));
}
