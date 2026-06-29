#ifndef PET_ACTION_CONTROLLER_H
#define PET_ACTION_CONTROLLER_H

#include <Arduino.h>
#include "domain/Animation.h"
#include "storage/AppearanceLoader.h"

class Pet;
class PetStorage;
class Renderer;

class PetActionController
{
public:
    PetActionController(Pet &pet, PetStorage &petStorage, Renderer &renderer, AppearanceLoader &appearanceLoader);

    bool loadOrDefault(const char *defaultSpeciesCode, const char *defaultOutfitCode);
    bool saveNow();
    void reset();
    void maybeSave();
    void decayEnvironment();
    bool dayPassed();
    bool applySpeciesForHealthyDays();
    bool applyAppearance(const char *speciesCode, const char *outfitCode);

    void feedPet(int addSatiety);
    void changeMood(int delta);
    bool takeMedicine();
    void takeShower(int value);
    void cleanEnvironment(unsigned int clearValue);
    void getSick();

    bool isFirstLaunchComplete() const;
    void markFirstLaunchComplete();
    void resetFirstLaunch();

    const char *speciesCode() const;
    const char *outfitCode() const;
    uint32_t healthyDays() const;
    AnimationId currentAnimation() const;
    AnimationId currentAgeAnimation() const;
    uint16_t currentAgeFrame(uint16_t maxFrame) const;
    uint16_t currentMoodFrame(uint16_t maxFrame) const;
    uint16_t currentHungerFrame(uint16_t maxFrame) const;

private:
    static constexpr uint8_t savePeriodTicks = 2;
    static constexpr unsigned int environmentDecayAmount = 5;

    Pet &pet;
    PetStorage &petStorage;
    Renderer &renderer;
    AppearanceLoader &appearanceLoader;
    uint8_t saveCounter = 0;
};

#endif // PET_ACTION_CONTROLLER_H
