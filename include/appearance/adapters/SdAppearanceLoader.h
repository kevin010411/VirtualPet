#ifndef SD_APPEARANCE_LOADER_H
#define SD_APPEARANCE_LOADER_H

#include <SdFat.h>
#include "appearance/ports/AppearanceLoader.h"
#include "pet_behavior/domain/PetBehaviorStatSlot.h"

class SdAppearanceLoader : public AppearanceLoader
{
public:
    explicit SdAppearanceLoader(SdFat *refSd);

    bool findInitialAppearance(AppearanceSelection &selection) override;
    bool findEvolutionTarget(const PetStatSnapshot &stats, AppearanceSelection &selection) override;
    bool loadSpecies(char species[][9], size_t maxSpecies, size_t &speciesCount) override;
    bool loadOutfits(const char *speciesCode, char outfits[][9], size_t maxOutfits, size_t &outfitCount) override;
    bool findOutfitPreview(const char *speciesCode, const char *outfitCode, OutfitPreview &preview) override;
    bool validateEvolutionContract(const PetBehaviorConfig &config) override;

private:
    ActivePetBehaviorStatSlots evolutionStatSlots;
    bool evolutionContractValidated = false;
    SdFat *sd;
};

#endif // SD_APPEARANCE_LOADER_H
