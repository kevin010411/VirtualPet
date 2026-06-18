#ifndef SD_APPEARANCE_LOADER_H
#define SD_APPEARANCE_LOADER_H

#include <SdFat.h>
#include "storage/AppearanceLoader.h"

class SdAppearanceLoader : public AppearanceLoader
{
public:
    explicit SdAppearanceLoader(SdFat *refSd);

    bool findForHealthyDays(uint32_t healthyDays, AppearanceSelection &selection) override;
    bool loadOutfits(const char *speciesCode, char outfits[][9], size_t maxOutfits, size_t &outfitCount) override;
    bool findOutfitPreview(const char *speciesCode, const char *outfitCode, OutfitPreview &preview) override;

private:
    SdFat *sd;
};

#endif // SD_APPEARANCE_LOADER_H
