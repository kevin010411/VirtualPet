#include "pet/application/PetActionController.h"

#include <string.h>
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

bool PetActionController::loadOrInitial(const AppearanceSelection &initialAppearance, uint32_t schemaFingerprint)
{
    const bool loaded = petStorage.load(pet, schemaFingerprint);
    if (!loaded)
    {
        pet.setDefaultState();
        pet.setSchemaFingerprint(schemaFingerprint);
        pet.setSpeciesSlot(initialAppearance.speciesSlot);
        pet.setOutfitSlot(initialAppearance.outfitSlot);
    }
    return loaded;
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

void PetActionController::reset()
{
    pet.setDefaultState();
    pet.resetFirstLaunch();
    saveCounter = 0;
}

void PetActionController::maybeSave()
{
    saveCounter += 1;
    if (saveCounter < savePeriodTicks)
        return;

    saveNow();
}

bool PetActionController::commitPetDay(const int16_t *customStats, size_t customStatCount)
{
    return pet.commitPetDay(customStats, customStatCount);
}

bool PetActionController::commitPetStats(const int16_t *customStats, size_t customStatCount)
{
    return pet.commitPetStats(customStats, customStatCount);
}

bool PetActionController::findEvolutionTarget(AppearanceSelection &selection) const
{
    if (!appearanceLoader.findEvolutionTarget(pet.statSnapshot(), selection))
        return false;

    return pet.speciesSlot() != selection.speciesSlot;
}

bool PetActionController::applyEvolutionTarget()
{
    AppearanceSelection selection = {};
    if (!findEvolutionTarget(selection))
        return false;

    return applyAppearance(selection.speciesSlot, selection.outfitSlot);
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

int16_t PetActionController::customStat(uint8_t index) const
{
    return pet.customStat(index);
}

bool PetActionController::setCustomStat(uint8_t index, int16_t value)
{
    return pet.setCustomStat(index, value);
}

bool PetActionController::changeCustomStat(uint8_t index, int16_t delta)
{
    return pet.changeCustomStat(index, delta);
}

bool PetActionController::changeCustomStatClamped(uint8_t index, int16_t delta, int16_t minValue, int16_t maxValue)
{
    return pet.changeCustomStatClamped(index, delta, minValue, maxValue);
}

bool PetActionController::isFirstLaunchComplete() const
{
    return pet.isFirstLaunchComplete();
}

void PetActionController::markFirstLaunchComplete()
{
    pet.markFirstLaunchComplete();
}

void PetActionController::resetFirstLaunch()
{
    pet.resetFirstLaunch();
}

bool PetActionController::isFirstStartCompleted() const
{
    return pet.isFirstStartCompleted();
}

void PetActionController::markFirstStartCompleted()
{
    pet.markFirstStartCompleted();
}

void PetActionController::resetFirstStartCompleted()
{
    pet.resetFirstStartCompleted();
}

uint8_t PetActionController::speciesSlot() const
{
    return pet.speciesSlot();
}

uint8_t PetActionController::outfitSlot() const
{
    return pet.outfitSlot();
}

PetStatSnapshot PetActionController::statSnapshot() const
{
    return pet.statSnapshot();
}
