#include "pet/application/PetActionController.h"

#include <stdio.h>
#include "pet/domain/Pet.h"
#include "presentation/adapters/rendering/Renderer.h"
#include "pet/adapters/PetStorage.h"

PetActionController::PetActionController(Pet &petRef, PetStorage &petStorageRef, Renderer &rendererRef, AppearanceLoader &appearanceLoaderRef)
    : pet(petRef),
      petStorage(petStorageRef),
      renderer(rendererRef),
      appearanceLoader(appearanceLoaderRef)
{
}

bool PetActionController::saveNow()
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

void PetActionController::maybeSave()
{
    saveCounter += 1;
    if (saveCounter < savePeriodTicks)
        return;

    saveNow();
}

EvolutionLookupResult PetActionController::findEvolutionTarget(AppearanceSelection &selection) const
{
    const EvolutionLookupResult result =
        appearanceLoader.findEvolutionTarget(pet.statSnapshot(), selection);
    if (result != EvolutionLookupResult::Found)
        return result;

    return pet.speciesSlot() != selection.speciesSlot
               ? EvolutionLookupResult::Found
               : EvolutionLookupResult::NoTarget;
}

bool PetActionController::stageAppearance(uint8_t speciesSlot, uint8_t outfitSlot)
{
    return pet.setSpeciesSlot(speciesSlot) && pet.setOutfitSlot(outfitSlot);
}

bool PetActionController::applyAppearance(uint8_t speciesSlot, uint8_t outfitSlot)
{
    if (!stageAppearance(speciesSlot, outfitSlot))
        return false;
    renderer.setAssetAppearance(pet.speciesSlot(), pet.outfitSlot());
    return saveNow();
}

bool PetActionController::applyConsumableOutfitUnlock(
    uint8_t outfitSlot, const PetStatSnapshot &consumedStats)
{
    if (!pet.commitConsumableOutfitUnlock(outfitSlot, consumedStats))
        return false;
    renderer.setAssetAppearance(pet.speciesSlot(), pet.outfitSlot());
    return saveNow();
}
