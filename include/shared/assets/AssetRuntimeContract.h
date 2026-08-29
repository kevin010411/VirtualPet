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
    bool animationIds[kMaxRuntimeAnimationId + 1] = {};
    uint8_t animationCount = 0;
    uint8_t speciesPackCount = 0;
};

bool sameBundleId(const BundleId &left, const BundleId &right);
bool parseBundleId(const char *text, BundleId &bundleId);
bool parseAnimationRef(const char *scope,
                       const char *animationId,
                       const RuntimeManifest &manifest,
                       AnimationRef &reference);
bool animationReferenceExists(BundleReader &reader,
                              const AnimationRef &reference,
                              uint8_t versionIndex = 0);
bool loadRuntimeManifest(SdFat *sd, RuntimeManifest &manifest);
} // namespace AssetData

#endif // ASSET_RUNTIME_CONTRACT_H
