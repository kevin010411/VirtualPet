#include "appearance/adapters/SdAppearanceLoader.h"

#include <string.h>
#include "appearance/domain/RuntimeTableAppearance.h"

namespace
{
void recordRuntimeResult(bool succeeded, BundleReader &reader, bool &lastSucceeded,
                         char *errorResource, size_t errorResourceSize)
{
    lastSucceeded = succeeded;
    if (succeeded)
    {
        errorResource[0] = '\0';
        return;
    }
    const char *pack = reader.firstErrorResource();
    const char *resource = pack != nullptr && pack[0] != '\0' ? pack : "runtime";
    strncpy(errorResource, resource, errorResourceSize - 1);
    errorResource[errorResourceSize - 1] = '\0';
}
} // namespace

SdAppearanceLoader::SdAppearanceLoader(SdFat *refSd)
    : sd(refSd), bundleReader(refSd, ioScratch, sizeof(ioScratch))
{
}

void SdAppearanceLoader::configureRuntimeContract(const PetBehaviorConfig &config)
{
    evolutionStatSlots.configure(config);
    assetManifest = config.assetManifest;
    bundleReader.configureBundle(assetManifest.bundleId);
    lastContractSucceeded = true;
    contractErrorResource[0] = '\0';
}

bool SdAppearanceLoader::validateRuntimeContracts(const PetStatSnapshot &stats)
{
    const bool valid = validateRuntimeTableAppearance(
        sd, assetManifest, bundleReader, evolutionStatSlots, stats);
    recordRuntimeResult(valid, bundleReader, lastContractSucceeded,
                        contractErrorResource, sizeof(contractErrorResource));
    return valid;
}

bool SdAppearanceLoader::lastContractLoadSucceeded() const
{
    return lastContractSucceeded;
}

const char *SdAppearanceLoader::firstAssetDataErrorResource() const
{
    const char *pack = bundleReader.firstErrorResource();
    return pack != nullptr && pack[0] != '\0' ? pack : contractErrorResource;
}

bool SdAppearanceLoader::findInitialAppearance(AppearanceSelection &selection)
{
    const bool loaded = loadRuntimeTableInitialAppearance(sd, assetManifest, bundleReader, selection);
    recordRuntimeResult(loaded, bundleReader, lastContractSucceeded,
                        contractErrorResource, sizeof(contractErrorResource));
    return loaded;
}

bool SdAppearanceLoader::findEvolutionTarget(const PetStatSnapshot &stats,
                                             AppearanceSelection &selection)
{
    const bool loaded = findRuntimeTableEvolutionTarget(
        sd, assetManifest, bundleReader, evolutionStatSlots, stats, selection);
    recordRuntimeResult(loaded, bundleReader, lastContractSucceeded,
                        contractErrorResource, sizeof(contractErrorResource));
    return loaded && selection.speciesSlot != 0;
}

bool SdAppearanceLoader::loadSpecies(uint8_t *species, size_t maxSpecies, size_t &speciesCount)
{
    const bool loaded = loadRuntimeTableSpecies(
        sd, assetManifest, bundleReader, species, maxSpecies, speciesCount);
    recordRuntimeResult(loaded, bundleReader, lastContractSucceeded,
                        contractErrorResource, sizeof(contractErrorResource));
    return loaded;
}

bool SdAppearanceLoader::loadOutfits(uint8_t speciesSlot, uint8_t unlockMask, uint8_t *outfits,
                                     size_t maxOutfits, size_t &outfitCount)
{
    const bool loaded = loadRuntimeTableOutfits(
        sd, assetManifest, bundleReader, speciesSlot, unlockMask, outfits, maxOutfits, outfitCount);
    recordRuntimeResult(loaded, bundleReader, lastContractSucceeded,
                        contractErrorResource, sizeof(contractErrorResource));
    return loaded;
}

bool SdAppearanceLoader::findOutfitPreview(uint8_t speciesSlot, uint8_t outfitSlot, bool locked,
                                           OutfitPreview &preview)
{
    const bool loaded = findRuntimeTableOutfitPreview(
        sd, assetManifest, bundleReader, speciesSlot, outfitSlot, locked, preview);
    recordRuntimeResult(loaded, bundleReader, lastContractSucceeded,
                        contractErrorResource, sizeof(contractErrorResource));
    return loaded;
}

bool SdAppearanceLoader::resolveOutfitUnlockMask(uint8_t speciesSlot, const PetStatSnapshot &stats,
                                                  uint8_t currentMask, bool initialize,
                                                  uint8_t &resolvedMask)
{
    const bool loaded = resolveRuntimeTableOutfitUnlockMask(
        sd, assetManifest, bundleReader, speciesSlot, evolutionStatSlots, stats,
        currentMask, initialize, resolvedMask);
    recordRuntimeResult(loaded, bundleReader, lastContractSucceeded,
                        contractErrorResource, sizeof(contractErrorResource));
    return loaded;
}
