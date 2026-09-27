#include "appearance/adapters/SdAppearanceLoader.h"

#include "appearance/domain/RuntimeTableAppearance.h"
#include "shared/utils/CopyResourceName.h"

SdAppearanceLoader::SdAppearanceLoader(SdFat *refSd)
    : sd(refSd), bundleReader(refSd, ioScratch, sizeof(ioScratch))
{
}

void SdAppearanceLoader::configureRuntimeContract(const PetBehaviorConfig &config)
{
    evolutionStatSlots.configure(config);
    assetManifest = config.assetManifest;
    bundleReader.configureBundle(assetManifest.bundleId);
    contractErrorResource[0] = '\0';
}

const char *SdAppearanceLoader::firstAssetDataErrorResource() const
{
    const char *pack = bundleReader.firstErrorResource();
    return pack != nullptr && pack[0] != '\0' ? pack : contractErrorResource;
}

bool SdAppearanceLoader::recordQueryResult(bool succeeded)
{
    if (succeeded)
    {
        contractErrorResource[0] = '\0';
        return true;
    }
    const char *pack = bundleReader.firstErrorResource();
    copyResourceName(contractErrorResource, sizeof(contractErrorResource),
                     pack != nullptr && pack[0] != '\0' ? pack : "runtime");
    return false;
}

EvolutionLookupResult SdAppearanceLoader::findEvolutionTarget(const PetStatSnapshot &stats,
                                                              AppearanceSelection &selection)
{
    const bool loaded = recordQueryResult(findRuntimeTableEvolutionTarget(
        sd, assetManifest, bundleReader, evolutionStatSlots, stats, selection));
    if (!loaded)
        return EvolutionLookupResult::LoadFailed;
    return selection.speciesSlot != 0 ? EvolutionLookupResult::Found
                                      : EvolutionLookupResult::NoTarget;
}

bool SdAppearanceLoader::loadSpecies(uint8_t *species, size_t maxSpecies, size_t &speciesCount)
{
    return recordQueryResult(loadRuntimeTableSpecies(
        sd, assetManifest, bundleReader, species, maxSpecies, speciesCount));
}

bool SdAppearanceLoader::loadOutfits(uint8_t speciesSlot, uint8_t unlockMask, uint8_t *outfits,
                                     size_t maxOutfits, size_t &outfitCount)
{
    return recordQueryResult(loadRuntimeTableOutfits(
        sd, assetManifest, bundleReader, speciesSlot, unlockMask, outfits, maxOutfits, outfitCount));
}

bool SdAppearanceLoader::findOutfitPreview(uint8_t speciesSlot, uint8_t outfitSlot, bool locked,
                                           OutfitPreview &preview)
{
    return recordQueryResult(findRuntimeTableOutfitPreview(
        sd, assetManifest, bundleReader, speciesSlot, outfitSlot, locked, preview));
}

bool SdAppearanceLoader::resolveOutfitUnlockMask(uint8_t speciesSlot, const PetStatSnapshot &stats,
                                                  uint8_t currentMask, bool initialize,
                                                  uint8_t &resolvedMask)
{
    return recordQueryResult(resolveRuntimeTableOutfitUnlockMask(
        sd, assetManifest, bundleReader, speciesSlot, evolutionStatSlots, stats,
        currentMask, initialize, resolvedMask));
}

bool SdAppearanceLoader::resolveConsumableOutfitUnlock(uint8_t speciesSlot, uint8_t outfitSlot,
                                                        const PetStatSnapshot &stats,
                                                        PetStatSnapshot &consumedStats)
{
    return recordQueryResult(resolveRuntimeTableConsumableOutfitUnlock(
        sd, assetManifest, bundleReader, speciesSlot, outfitSlot,
        evolutionStatSlots, stats, consumedStats));
}
