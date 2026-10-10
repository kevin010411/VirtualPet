#ifndef PET_SAVE_CONTROLLER_H
#define PET_SAVE_CONTROLLER_H

#include <stdint.h>

class Pet;
class PetStorage;
class Renderer;
struct PetBehaviorConfig;

class PetSaveController
{
public:
    PetSaveController(Pet &pet, PetStorage &storage, Renderer &renderer);
    bool saveNow();
    // Immediate event commits bypass a periodic RTC catch-up save batch.
    bool saveCollectionProgress(bool forceSave, bool immediate = false);
    bool applyCollectionGrants();
    void configureRuntimeContract(const PetBehaviorConfig &value) { config = &value; }
    void maybeSave();

private:
    static constexpr uint8_t savePeriodTicks = 2;
    Pet &pet;
    PetStorage &petStorage;
    Renderer &renderer;
    uint8_t saveCounter = 0;
    const PetBehaviorConfig *config = nullptr;
    bool saveSnapshot(bool immediate = false);
};

#endif
