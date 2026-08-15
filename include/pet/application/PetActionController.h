#ifndef PET_ACTION_CONTROLLER_H
#define PET_ACTION_CONTROLLER_H

#include <Arduino.h>
#include "animation/domain/Animation.h"
#include "pet/domain/Pet.h"
#include "appearance/ports/AppearanceLoader.h"

class Pet;
class PetStorage;
class Renderer;

class PetActionController
{
public:
    PetActionController(Pet &pet, PetStorage &petStorage, Renderer &renderer, AppearanceLoader &appearanceLoader);

    bool loadOrInitial(const AppearanceSelection &initialAppearance);
    bool saveNow();
    void reset();
    void maybeSave();
    void decayEnvironment();
    void dayPassed();
    bool commitPetStats(const int16_t *customStats, size_t customStatCount);
    bool commitPetDay(const int16_t *customStats, size_t customStatCount);
    bool findEvolutionTarget(AppearanceSelection &selection) const;
    bool applyEvolutionTarget();
    bool applyAppearance(const char *speciesCode, const char *outfitCode);

    void feedPet(int addSatiety);
    void changeMood(int delta);
    bool takeMedicine();
    void takeShower(int value);
    void cleanEnvironment(unsigned int clearValue);
    void getSick();
    int16_t customStat(uint8_t index) const;
    bool setCustomStat(uint8_t index, int16_t value);
    bool changeCustomStat(uint8_t index, int16_t delta);
    bool changeCustomStatClamped(uint8_t index, int16_t delta, int16_t minValue, int16_t maxValue);

    bool isFirstLaunchComplete() const;
    void markFirstLaunchComplete();
    void resetFirstLaunch();

    const char *speciesCode() const;
    const char *outfitCode() const;
    HealthStatus currentStatus() const;
    bool isMoodDepressed() const;
    AnimationId currentAnimation() const;
    PetStatSnapshot statSnapshot() const;

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
