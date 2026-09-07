#ifndef SD_APPEARANCE_LOADER_H
#define SD_APPEARANCE_LOADER_H

#include <SdFat.h>
#include "appearance/ports/AppearanceLoader.h"
#include "pet_behavior/domain/PetBehaviorStatSlot.h"

class SdAppearanceLoader : public AppearanceLoader
{
public:
    explicit SdAppearanceLoader(SdFat *refSd);

    void configureRuntimeContract(const PetBehaviorConfig &config) override;
    bool validateRuntimeContracts(const PetStatSnapshot &stats) override;
    bool lastContractLoadSucceeded() const override;
    const char *firstAssetDataErrorResource() const override;
    bool findInitialAppearance(AppearanceSelection &selection) override;
    bool findEvolutionTarget(const PetStatSnapshot &stats, AppearanceSelection &selection) override;
    bool loadSpecies(uint8_t *species, size_t maxSpecies, size_t &speciesCount) override;
    bool loadOutfits(uint8_t speciesSlot, uint8_t unlockMask, uint8_t *outfits, size_t maxOutfits, size_t &outfitCount) override;
    bool findOutfitPreview(uint8_t speciesSlot, uint8_t outfitSlot, bool locked, OutfitPreview &preview) override;
    bool resolveOutfitUnlockMask(uint8_t speciesSlot, const PetStatSnapshot &stats,
                                 uint8_t currentMask, bool initialize,
                                 uint8_t &resolvedMask) override;
    bool resolveConsumableOutfitUnlock(uint8_t speciesSlot, uint8_t outfitSlot,
                                        const PetStatSnapshot &stats,
                                        PetStatSnapshot &consumedStats) override;
private:
    SdFat *sd;
    uint8_t ioScratch[AssetData::kIoScratchBytes] = {};
    BundleReader bundleReader;
    ActivePetBehaviorStatSlots evolutionStatSlots;
    AssetData::RuntimeManifest assetManifest;
    bool lastContractSucceeded = true;
    char contractErrorResource[20] = {};
};

#endif // SD_APPEARANCE_LOADER_H
