#ifndef APPEARANCE_LOADER_H
#define APPEARANCE_LOADER_H

#include <Arduino.h>

struct AppearanceSelection
{
    char speciesCode[9];
    char outfitCode[9];
};

class AppearanceLoader
{
public:
    virtual ~AppearanceLoader() = default;
    virtual bool findForHealthyDays(uint32_t healthyDays, AppearanceSelection &selection) = 0;
};

#endif // APPEARANCE_LOADER_H
