#include "pet/application/PetActionController.h"

#include <string.h>
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

bool PetActionController::loadOrDefault(const char *defaultSpeciesCode, const char *defaultOutfitCode)
{
    const bool loaded = petStorage.load(pet);
    if (!loaded)
    {
        pet.setDefaultState();
        pet.setSpeciesCode(defaultSpeciesCode);
        pet.setOutfitCode(defaultOutfitCode);
    }
    return loaded;
}

bool PetActionController::saveNow()
{
    const bool saved = petStorage.save(pet);
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

bool PetActionController::dayPassed()
{
    return pet.dayPassed();
}

bool PetActionController::findSpeciesForHealthyDays(AppearanceSelection &selection) const
{
    if (!appearanceLoader.findForHealthyDays(pet.healthyDays(), selection))
        return false;

    return strcmp(pet.speciesCode(), selection.speciesCode) != 0;
}

bool PetActionController::applySpeciesForHealthyDays()
{
    AppearanceSelection selection = {};
    if (!findSpeciesForHealthyDays(selection))
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

uint32_t PetActionController::healthyDays() const
{
    return pet.healthyDays();
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

AnimationId PetActionController::currentAgeAnimation() const
{
    return pet.CurrentAgeAnimation();
}

uint16_t PetActionController::currentAgeFrame(uint16_t maxFrame) const
{
    return pet.CurrentAgeFrame(maxFrame);
}

uint16_t PetActionController::currentMoodFrame(uint16_t maxFrame) const
{
    return pet.CurrentMoodFrame(maxFrame);
}

uint16_t PetActionController::currentHungerFrame(uint16_t maxFrame) const
{
    return pet.CurrentHungerFrame(maxFrame);
}
