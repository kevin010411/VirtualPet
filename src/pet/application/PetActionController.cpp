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

bool PetActionController::loadOrInitial(const AppearanceSelection &initialAppearance)
{
    const bool loaded = petStorage.load(pet);
    if (!loaded)
    {
        pet.setDefaultState();
        pet.setSpeciesCode(initialAppearance.speciesCode);
        pet.setOutfitCode(initialAppearance.outfitCode);
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
    snprintf(detail, sizeof(detail), "%s/%s", pet.speciesCode(), pet.outfitCode());
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

void PetActionController::decayEnvironment()
{
    pet.decayEnvironment(environmentDecayAmount);
}

void PetActionController::dayPassed()
{
    pet.dayPassed();
}

bool PetActionController::findEvolutionTarget(AppearanceSelection &selection) const
{
    if (!appearanceLoader.findEvolutionTarget(pet.statSnapshot(), selection))
        return false;

    return strcmp(pet.speciesCode(), selection.speciesCode) != 0;
}

bool PetActionController::applyEvolutionTarget()
{
    AppearanceSelection selection = {};
    if (!findEvolutionTarget(selection))
        return false;

    return applyAppearance(selection.speciesCode, selection.outfitCode);
}

bool PetActionController::applyAppearance(const char *speciesCode, const char *outfitCode)
{
    if (!pet.setSpeciesCode(speciesCode) || !pet.setOutfitCode(outfitCode))
        return false;

    renderer.setAssetAppearance(pet.speciesCode(), pet.outfitCode());
    renderer.reloadManifest();
    return saveNow();
}

void PetActionController::feedPet(int addSatiety)
{
    pet.feedPet(addSatiety);
}

void PetActionController::changeMood(int delta)
{
    pet.changeMood(delta);
}

bool PetActionController::takeMedicine()
{
    return pet.takeMedicine();
}

void PetActionController::takeShower(int value)
{
    pet.takeShower(value);
}

void PetActionController::cleanEnvironment(unsigned int clearValue)
{
    pet.cleanEnv(clearValue);
}

void PetActionController::getSick()
{
    pet.getSick();
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

const char *PetActionController::speciesCode() const
{
    return pet.speciesCode();
}

const char *PetActionController::outfitCode() const
{
    return pet.outfitCode();
}

HealthStatus PetActionController::currentStatus() const
{
    return pet.getStatus();
}

bool PetActionController::isMoodDepressed() const
{
    return pet.isMoodDepressed();
}

AnimationId PetActionController::currentAnimation() const
{
    return pet.CurrentAnimation();
}

PetStatSnapshot PetActionController::statSnapshot() const
{
    return pet.statSnapshot();
}
