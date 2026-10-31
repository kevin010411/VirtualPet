#include "resources/RuntimeContractLoader.h"

#include "appearance/RuntimeTableAppearance.h"
#include "resources/RuntimeTableBehavior.h"
#include "common/CopyResourceName.h"

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
    const char *fallback = "asset data";
    if (bundleReader.configureBundle(manifest.bundleId))
    {
        if (loadCompleteRuntimeTable(sd, manifest, bundleReader,
                                     speciesSlot, outfitSlot, config,
                                     selection, initialAppearanceResolved))
            return true;
        fallback = "runtime.bin";
    }
    else
    {
        config = {};
        if (selection != nullptr) *selection = {};
        if (initialAppearanceResolved != nullptr) *initialAppearanceResolved = false;
    }
    const char *resource = bundleReader.firstErrorResource();
    copyResourceName(errorResource, errorResourceCapacity,
                     resource != nullptr && resource[0] != '\0' ? resource : fallback);
    return false;
}

} // namespace

bool loadRuntimeContract(SdFat *sd,
                         uint8_t speciesSlot,
                         uint8_t outfitSlot,
                         PetBehaviorConfig &config,
                         char *errorResource,
                         size_t errorResourceCapacity)
{
    if (errorResource != nullptr && errorResourceCapacity != 0)
        errorResource[0] = '\0';

    AssetData::RuntimeManifest manifest = {};
    if (speciesSlot == 0 || outfitSlot == 0 || !loadRuntimeManifest(sd, manifest))
    {
        config = {};
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
    if (errorResource != nullptr && errorResourceCapacity != 0)
        errorResource[0] = '\0';
    AssetData::RuntimeManifest manifest = {};
    if (!loadRuntimeManifest(sd, manifest))
    {
        config = {};
        selection = {};
        initialAppearanceResolved = false;
        copyResourceName(errorResource, errorResourceCapacity, "runtime.bin");
        return false;
    }
    return loadFromManifest(sd, manifest, 0, 0, config, &selection,
                            &initialAppearanceResolved, errorResource, errorResourceCapacity);
}
