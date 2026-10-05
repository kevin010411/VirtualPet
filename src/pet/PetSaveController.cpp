#include "pet/PetSaveController.h"

#include <stdio.h>
#include "pet/Pet.h"
#include "display/Renderer.h"
#include "pet/PetStorage.h"

PetSaveController::PetSaveController(Pet &petRef, PetStorage &petStorageRef, Renderer &rendererRef)
    : pet(petRef),
      petStorage(petStorageRef),
      renderer(rendererRef)
{
}

bool PetSaveController::saveNow()
{
    const bool saved = petStorage.save(pet);
#if ENABLE_DEBUG
    char title[21] = {};
    char detail[21] = {};
    snprintf(
        title,
        sizeof(title),
        "SAVE %s %c %lu",
        saved ? "OK" : "FAIL",
        petStorage.lastSaveSlot(),
        static_cast<unsigned long>(petStorage.lastSaveSequence()));
    snprintf(detail, sizeof(detail), "%u/%u", pet.speciesSlot(), pet.outfitSlot());
    renderer.debugDisplay().showMessage(title, detail);
#endif
    if (saved)
        saveCounter = 0;
    return saved;
}

void PetSaveController::maybeSave()
{
    saveCounter += 1;
    if (saveCounter < savePeriodTicks)
        return;

    saveNow();
}
