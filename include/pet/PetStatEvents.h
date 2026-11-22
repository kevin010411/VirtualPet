#ifndef PET_STAT_EVENTS_H
#define PET_STAT_EVENTS_H

#include "pet/Pet.h"
#include "pet/PetBehaviorTypes.h"

#if ENABLE_STAT_EVENTS
#include "pet/StatEventSettings.h"

constexpr uint8_t kMaxStatEventPages = 64;

struct PetStatEventResult
{
    PetStatSnapshot values;
    bool terminal;
    uint16_t pages[kMaxStatEventPages];
    uint8_t pageCount;
};

bool settleStatEvents(SdFat *sd, const StatEventSettings &events, const PetBehaviorConfig &config,
                      const PetStatSnapshot &before, const PetStatSnapshot &after,
                      PetStatEventResult &result);
bool readStatEventPage(SdFat *sd, const StatEventSettings &events, uint16_t index,
                       uint16_t &animationId, uint16_t &durationMs);
#endif

#endif // PET_STAT_EVENTS_H
