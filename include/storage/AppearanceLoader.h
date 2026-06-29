#ifndef APPEARANCE_LOADER_H
#define APPEARANCE_LOADER_H

#include <Arduino.h>
#include <stddef.h>

struct AppearanceSelection
{
    char speciesCode[9];
    char outfitCode[9];
};

struct OutfitPreview
{
    char outfitCode[9];
    char path[96];
    uint16_t width;
    uint16_t height;
    uint16_t frameCount;
    uint16_t frameIntervalMs;
};

class AppearanceLoader
{
public:
    virtual ~AppearanceLoader() = default;
    virtual bool findForHealthyDays(uint32_t healthyDays, AppearanceSelection &selection) = 0;
    virtual bool loadSpecies(char species[][9], size_t maxSpecies, size_t &speciesCount) = 0;
    virtual bool loadOutfits(const char *speciesCode, char outfits[][9], size_t maxOutfits, size_t &outfitCount) = 0;
    virtual bool findOutfitPreview(const char *speciesCode, const char *outfitCode, OutfitPreview &preview) = 0;
};

#endif // APPEARANCE_LOADER_H
