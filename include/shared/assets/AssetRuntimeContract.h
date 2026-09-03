#ifndef ASSET_RUNTIME_CONTRACT_H
#define ASSET_RUNTIME_CONTRACT_H

#include <SdFat.h>
#include <stdint.h>
#include "shared/assets/BundleReader.h"

namespace AssetData
{
struct AnimationRef
{
    uint8_t speciesSlot = 0;
    uint8_t outfitSlot = 0;
    uint16_t animationId = 0;

    bool valid() const
    {
        return animationId > 0 && animationId <= kMaxRuntimeAnimationId &&
               ((speciesSlot == 0 && outfitSlot == 0) ||
                (speciesSlot != 0 && outfitSlot != 0));
    }

    bool shared() const { return speciesSlot == 0; }
};

struct RuntimeManifest
{
    BundleId bundleId = {};
    uint32_t schemaFingerprint = 0;
    uint32_t fileSize = 0;
    uint32_t fileCrc32 = 0;
};

bool sameBundleId(const BundleId &left, const BundleId &right);
bool animationReferenceExists(BundleReader &reader,
                              const AnimationRef &reference,
                              uint8_t versionIndex = 0);
} // namespace AssetData

#endif // ASSET_RUNTIME_CONTRACT_H
