#include "pet_behavior/domain/RuntimeContractLoader.h"

#include <string.h>
#include "appearance/domain/RuntimeTableAppearance.h"
#include "pet_behavior/domain/RuntimeTableBehavior.h"
#include "shared/assets/AssetRuntimeContract.h"

namespace
{
void copyLoadError(char *destination, size_t capacity, const char *resource)
{
    if (destination == nullptr || capacity == 0)
        return;
    strncpy(destination, resource, capacity - 1);
    destination[capacity - 1] = '\0';
}
} // namespace

bool loadRuntimeContract(SdFat *sd,
                         uint8_t speciesSlot,
                         uint8_t outfitSlot,
                         PetBehaviorConfig &config,
                         char *errorResource,
                         size_t errorResourceCapacity)
{
    config = {};
    if (errorResource != nullptr && errorResourceCapacity != 0)
        errorResource[0] = '\0';

    AssetData::RuntimeManifest manifest = {};
    if (speciesSlot == 0 || outfitSlot == 0 || !AssetData::loadRuntimeManifest(sd, manifest))
    {
        copyLoadError(errorResource, errorResourceCapacity, "asset_manifest");
        return false;
    }

    uint8_t verificationScratch[AssetData::kVerificationScratchBytes] = {};
    BundleReader bundleReader(sd, verificationScratch, sizeof(verificationScratch));
    if (!bundleReader.configureBundle(manifest.bundleId))
    {
        const char *resource = bundleReader.firstErrorResource();
        copyLoadError(errorResource, errorResourceCapacity,
                      resource != nullptr && resource[0] != '\0' ? resource : "asset data");
        return false;
    }

    PetBehaviorConfig candidate = {};
    if (!loadCompleteRuntimeTable(sd, manifest, bundleReader,
                                  speciesSlot, outfitSlot, candidate))
    {
        const char *resource = bundleReader.firstErrorResource();
        copyLoadError(errorResource, errorResourceCapacity,
                      resource != nullptr && resource[0] != '\0' ? resource : "runtime.bin");
        return false;
    }

    config = candidate;
    return true;
}
