#ifndef BUNDLE_READER_H
#define BUNDLE_READER_H

#include <Arduino.h>
#include <SdFat.h>

namespace AssetData
{
constexpr uint8_t kPackKindSpecies = 1;
constexpr uint8_t kPackKindShared = 2;
constexpr uint8_t kCodecPal8RunLiteral = 1;
constexpr uint8_t kCodecRgb565RunLiteral = 2;
constexpr uint16_t kVersion = 1;
constexpr uint16_t kHeaderSize = 80;
constexpr uint16_t kAnimationRecordSize = 16;
constexpr uint16_t kFrameRecordSize = 24;
constexpr uint16_t kMaxRuntimeAnimationId = 96;
constexpr uint16_t kMaxAnimationRecords = 4096;
constexpr uint16_t kMaxFrameDescriptors = 65535;
constexpr uint16_t kMaxFramesPerAnimation = 256;
constexpr uint8_t kMaxVersions = 4;
constexpr uint16_t kMaxWidth = 128;
constexpr uint16_t kMaxHeight = 96;
constexpr uint32_t kMaxDecodedBytes = 24576;
constexpr uint32_t kMaxPackBytes = 20UL * 1024UL * 1024UL;
constexpr size_t kVerificationScratchBytes = 512;

struct BundleId
{
    uint8_t bytes[16] = {};
};

struct AssetFrameAddress
{
    uint8_t speciesSlot = 0;
    uint8_t outfitSlot = 0;
    uint16_t animationId = 0;
    uint8_t versionIndex = 0;
    uint16_t frameIndex = 0;
};

struct AnimationRecord
{
    uint16_t animationId = 0;
    uint8_t outfitSlot = 0;
    uint8_t versionIndex = 0;
    uint32_t firstFrame = 0;
    uint16_t frameCount = 0;
    uint16_t frameMs = 0;
};

struct FrameDescriptor
{
    uint8_t codec = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    uint32_t payloadOffset = 0;
    uint32_t encodedSize = 0;
    uint32_t decodedSize = 0;
};

struct OpenFrame
{
    SdBaseFile file;
    FrameDescriptor descriptor;
    uint16_t animationFrameCount = 0;
    uint16_t frameMs = 0;
};

enum class BundleError : uint8_t
{
    None,
    NotConfigured,
    InvalidAddress,
    MissingPack,
    InvalidPack,
    MissingAnimation,
    InvalidFrame,
    DecodeFailed,
};
} // namespace AssetData

class BundleReader
{
public:
    BundleReader(SdFat *sd, uint8_t *scratch, size_t scratchSize);

    bool configureBundle(const AssetData::BundleId &bundleId);
    bool resolveAnimation(const AssetData::AssetFrameAddress &address,
                          AssetData::AnimationRecord &animation);
    bool tryResolveAnimation(const AssetData::AssetFrameAddress &address,
                             AssetData::AnimationRecord &animation);
    bool openFrame(const AssetData::AssetFrameAddress &address,
                   AssetData::OpenFrame &frame);
    void rejectDecodedFrame(const AssetData::AssetFrameAddress &address);

    AssetData::BundleError firstError() const;
    const char *firstErrorResource() const;

private:
    struct PackHeader;

    enum class VerificationState : uint8_t
    {
        Unknown = 0,
        Verified = 1,
        Failed = 2,
    };

    bool validateAddress(const AssetData::AssetFrameAddress &address);
    bool buildPackPath(uint8_t speciesSlot, char *path, size_t pathSize) const;
    bool openVerifiedPack(uint8_t speciesSlot, SdBaseFile &file, PackHeader &header);
    bool verifyPack(SdBaseFile &file, uint8_t speciesSlot, PackHeader &header);
    bool readHeader(SdBaseFile &file, PackHeader &header);
    bool validateHeader(const PackHeader &header, uint8_t speciesSlot, uint32_t physicalSize) const;
    bool validateAnimations(SdBaseFile &file, const PackHeader &header);
    bool validateAnimationRanges(SdBaseFile &file, const PackHeader &header);
    bool validateDescriptors(SdBaseFile &file, const PackHeader &header);
    bool validateCrc(SdBaseFile &file, const PackHeader &header);
    bool findAnimation(SdBaseFile &file,
                       const PackHeader &header,
                       const AssetData::AssetFrameAddress &address,
                       AssetData::AnimationRecord &animation);
    bool readAnimation(SdBaseFile &file,
                       const PackHeader &header,
                       uint32_t index,
                       AssetData::AnimationRecord &animation) const;
    bool readDescriptor(SdBaseFile &file,
                        const PackHeader &header,
                        uint32_t index,
                        AssetData::FrameDescriptor &descriptor) const;
    bool recordError(AssetData::BundleError error, uint8_t speciesSlot);
    VerificationState verificationState(uint8_t speciesSlot) const;
    void setVerificationState(uint8_t speciesSlot, VerificationState state);

    SdFat *sd_;
    uint8_t *scratch_;
    size_t scratchSize_;
    AssetData::BundleId bundleId_;
    bool bundleConfigured_ = false;
    uint8_t verificationStates_[64] = {};
    uint32_t verifiedCrc_[256] = {};
    AssetData::BundleError firstError_ = AssetData::BundleError::None;
    char firstErrorResource_[20] = {};
};

#endif
