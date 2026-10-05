#ifndef PET_SAVE_CONTROLLER_H
#define PET_SAVE_CONTROLLER_H

#include <stdint.h>

class Pet;
class PetStorage;
class Renderer;

class PetSaveController
{
public:
    PetSaveController(Pet &pet, PetStorage &storage, Renderer &renderer);
    bool saveNow();
    void maybeSave();

private:
    static constexpr uint8_t savePeriodTicks = 2;
    Pet &pet;
    PetStorage &petStorage;
    Renderer &renderer;
    uint8_t saveCounter = 0;
};

#endif
