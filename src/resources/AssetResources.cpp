#include "resources/AssetResources.h"
#include "common/CopyResourceName.h"

namespace
{
AssetData::AssetFrameAddress frameAddress(const AssetData::AnimationRef &animation,
                                          uint8_t version, uint16_t frame)
{
    return {animation.speciesSlot, animation.outfitSlot, animation.animationId, version, frame};
}
}

AssetResources::AssetResources(SdFat *sd)
    : sd_(sd), reader_(sd, readBuffer_, AssetData::kIoScratchBytes)
{
}

bool AssetResources::configureBundle(const AssetData::BundleId &bundleId)
{
    return reader_.configureBundle(bundleId);
}

bool AssetResources::resolveAnimation(const AssetData::AnimationRef &animation,
                                       uint8_t versionIndex, AssetData::AnimationRecord &record)
{
    return animation.valid() && reader_.resolveAnimation(frameAddress(animation, versionIndex, 0), record);
}

uint16_t AssetResources::frameCountFor(const AssetData::AnimationRef &animation,
                                 uint8_t versionIndex)
{
    AssetData::AnimationRecord record = {};
    return animation.valid() &&
                   reader_.tryResolveAnimation(frameAddress(animation, versionIndex, 0), record)
               ? record.frameCount
               : 0;
}

uint16_t AssetResources::versionCountFor(const AssetData::AnimationRef &animation)
{
    uint16_t count = 0;
    for (uint16_t version = 0; version < AssetData::kMaxVersions; ++version)
    {
        AssetData::AnimationRecord record = {};
        if (!animation.valid() ||
            !reader_.tryResolveAnimation(
                frameAddress(animation, static_cast<uint8_t>(version), 0), record))
            break;
        ++count;
    }
    return count;
}

unsigned long AssetResources::frameIntervalFor(const AssetData::AnimationRef &animation,
                                         uint8_t versionIndex,
                                         unsigned long defaultIntervalMs)
{
    AssetData::AnimationRecord record = {};
    if (!animation.valid() ||
        !reader_.tryResolveAnimation(frameAddress(animation, versionIndex, 0), record) ||
        record.frameMs == 0)
        return defaultIntervalMs;
    return static_cast<unsigned long>(record.frameMs);
}

void AssetResources::recordAssetDataErrorResource(const char *resource)
{
    if (externalErrorResource_[0] != '\0' || resource == nullptr || resource[0] == '\0')
        return;
    copyResourceName(externalErrorResource_, sizeof(externalErrorResource_), resource);
}

AssetData::BundleError AssetResources::firstAssetDataError() const
{
    const AssetData::BundleError readerError = reader_.firstError();
    return readerError != AssetData::BundleError::None
               ? readerError
               : (externalErrorResource_[0] != '\0'
                      ? AssetData::BundleError::InvalidPack
                      : AssetData::BundleError::None);
}

const char *AssetResources::firstAssetDataErrorResource() const
{
    const char *pack = reader_.firstErrorResource();
    return pack != nullptr && pack[0] != '\0' ? pack : externalErrorResource_;
}
