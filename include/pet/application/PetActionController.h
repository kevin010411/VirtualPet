#ifndef PET_ACTION_CONTROLLER_H
#define PET_ACTION_CONTROLLER_H

#include <Arduino.h>
#include "pet/domain/Pet.h"
#include "appearance/ports/AppearanceLoader.h"

class Pet;
class PetStorage;
class Renderer;

class PetActionController
{
public:
    PetActionController(Pet &pet, PetStorage &petStorage, Renderer &renderer, AppearanceLoader &appearanceLoader);

    bool saveNow();
    void maybeSave();
    EvolutionLookupResult findEvolutionTarget(AppearanceSelection &selection) const;
    bool stageAppearance(uint8_t speciesSlot, uint8_t outfitSlot);
    bool applyAppearance(uint8_t speciesSlot, uint8_t outfitSlot);
    bool applyConsumableOutfitUnlock(uint8_t outfitSlot,
                                     const PetStatSnapshot &consumedStats);

private:
    static constexpr uint8_t savePeriodTicks = 2;
    Pet &pet;
    PetStorage &petStorage;
    Renderer &renderer;
    AppearanceLoader &appearanceLoader;
    uint8_t saveCounter = 0;
};

#endif // PET_ACTION_CONTROLLER_H
