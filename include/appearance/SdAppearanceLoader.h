#ifndef SD_APPEARANCE_LOADER_H
#define SD_APPEARANCE_LOADER_H

#include <SdFat.h>
#include "appearance/AppearanceLoader.h"
#include "pet/PetBehaviorStatSlot.h"

class SdAppearanceLoader : public AppearanceLoader
{
public:
    explicit SdAppearanceLoader(SdFat *refSd);

    void configureRuntimeContract(const PetBehaviorConfig &config) override;
    const char *firstAssetDataErrorResource() const override;
    EvolutionLookupResult findEvolutionTarget(const PetStatSnapshot &stats,
                                             AppearanceSelection &selection) override;
    bool loadOutfits(uint8_t speciesSlot, uint8_t unlockMask, uint8_t *outfits, size_t maxOutfits, size_t &outfitCount) override;
    bool findOutfitPreview(uint8_t speciesSlot, uint8_t outfitSlot, bool locked, OutfitPreview &preview) override;
    bool resolveOutfitUnlockMask(uint8_t speciesSlot, const PetStatSnapshot &stats,
                                 uint8_t currentMask, bool initialize,
                                 uint8_t &resolvedMask) override;
    bool resolveConsumableOutfitUnlock(uint8_t speciesSlot, uint8_t outfitSlot,
                                        const PetStatSnapshot &stats,
                                        PetStatSnapshot &consumedStats) override;
private:
    bool recordQueryResult(bool succeeded);

    SdFat *sd;
    uint8_t ioScratch[AssetData::kIoScratchBytes] = {};
    BundleReader bundleReader;
    ActivePetBehaviorStatSlots evolutionStatSlots;
    AssetData::RuntimeManifest assetManifest;
    char contractErrorResource[20] = {};
};

#endif // SD_APPEARANCE_LOADER_H
