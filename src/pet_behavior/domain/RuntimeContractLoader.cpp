#include "pet_behavior/domain/RuntimeContractLoader.h"

#include "appearance/domain/RuntimeTableAppearance.h"
#include "pet_behavior/domain/RuntimeTableBehavior.h"
#include "shared/utils/CopyResourceName.h"

namespace
{
bool loadFromManifest(SdFat *sd,
                      const AssetData::RuntimeManifest &manifest,
                      uint8_t speciesSlot,
                      uint8_t outfitSlot,
                      PetBehaviorConfig &config,
                      AppearanceSelection *selection,
                      bool *initialAppearanceResolved,
                      char *errorResource,
                      size_t errorResourceCapacity)
{
    uint8_t ioScratch[AssetData::kIoScratchBytes] = {};
    BundleReader bundleReader(sd, ioScratch, sizeof(ioScratch));
    if (!bundleReader.configureBundle(manifest.bundleId))
    {
        const char *resource = bundleReader.firstErrorResource();
        copyResourceName(errorResource, errorResourceCapacity,
                      resource != nullptr && resource[0] != '\0' ? resource : "asset data");
        return false;
    }

    if (!loadCompleteRuntimeTable(sd, manifest, bundleReader,
                                  speciesSlot, outfitSlot, config,
                                  selection, initialAppearanceResolved))
    {
        const char *resource = bundleReader.firstErrorResource();
        copyResourceName(errorResource, errorResourceCapacity,
                      resource != nullptr && resource[0] != '\0' ? resource : "runtime.bin");
        return false;
    }
    return true;
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
    if (speciesSlot == 0 || outfitSlot == 0 || !loadRuntimeManifest(sd, manifest))
    {
        copyResourceName(errorResource, errorResourceCapacity, "runtime.bin");
        return false;
    }
    return loadFromManifest(sd, manifest, speciesSlot, outfitSlot, config,
                            nullptr, nullptr, errorResource, errorResourceCapacity);
}

bool loadInitialRuntimeContract(SdFat *sd,
                                AppearanceSelection &selection,
                                PetBehaviorConfig &config,
                                bool &initialAppearanceResolved,
                                char *errorResource,
                                size_t errorResourceCapacity)
{
    config = {};
    selection = {};
    initialAppearanceResolved = false;
    if (errorResource != nullptr && errorResourceCapacity != 0)
        errorResource[0] = '\0';
    AssetData::RuntimeManifest manifest = {};
    if (!loadRuntimeManifest(sd, manifest))
    {
        copyResourceName(errorResource, errorResourceCapacity, "runtime.bin");
        return false;
    }
    return loadFromManifest(sd, manifest, 0, 0, config, &selection,
                            &initialAppearanceResolved, errorResource, errorResourceCapacity);
}
