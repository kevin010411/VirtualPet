#ifndef ASSET_RESOURCES_H
#define ASSET_RESOURCES_H

#include "resources/AssetRuntimeContract.h"

// One bundle/cache and one read buffer for synchronous SD resource access.
// FrameDecoder borrows the buffer only for the duration of a draw call.
class AssetResources
{
public:
    explicit AssetResources(SdFat *sd);
    AssetResources(const AssetResources &) = delete;
    AssetResources &operator=(const AssetResources &) = delete;

    bool configureBundle(const AssetData::BundleId &bundleId);
    bool resolveAnimation(const AssetData::AnimationRef &animation,
                          uint8_t versionIndex, AssetData::AnimationRecord &record);
    uint16_t frameCountFor(const AssetData::AnimationRef &animation, uint8_t versionIndex = 0);
    uint16_t versionCountFor(const AssetData::AnimationRef &animation);
    unsigned long frameIntervalFor(const AssetData::AnimationRef &animation,
                                   uint8_t versionIndex, unsigned long defaultIntervalMs);
    void recordAssetDataErrorResource(const char *resource);
    AssetData::BundleError firstAssetDataError() const;
    const char *firstAssetDataErrorResource() const;
    SdFat *sdCard() const { return sd_; }

    BundleReader &frameReader() { return reader_; }
    const BundleReader &frameReader() const { return reader_; }
    uint8_t *readBuffer() { return readBuffer_; }

private:
    SdFat *sd_;
    alignas(uint16_t) uint8_t readBuffer_[AssetData::kDataReadBufferBytes] = {};
    BundleReader reader_;
    char externalErrorResource_[20] = {};
};

#endif
