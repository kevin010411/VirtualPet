#include "shared/assets/AssetRuntimeContract.h"

#include <string.h>

namespace AssetData
{
bool sameBundleId(const BundleId &left, const BundleId &right)
{
    return memcmp(left.bytes, right.bytes, sizeof(left.bytes)) == 0;
}

bool animationReferenceExists(BundleReader &reader,
                              const AnimationRef &reference,
                              uint8_t versionIndex)
{
    if (!reference.valid() || versionIndex >= kMaxVersions)
        return false;
    AssetFrameAddress address = {};
    address.speciesSlot = reference.speciesSlot;
    address.outfitSlot = reference.outfitSlot;
    address.animationId = reference.animationId;
    address.versionIndex = versionIndex;
    AnimationRecord animation = {};
    return reader.resolveAnimation(address, animation);
}
} // namespace AssetData
