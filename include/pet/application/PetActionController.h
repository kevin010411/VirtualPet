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

    bool loadOrInitial(const AppearanceSelection &initialAppearance, uint32_t schemaFingerprint);
    bool saveNow();
    void reset();
    void maybeSave();
    bool commitPetStats(const int16_t *customStats, size_t customStatCount);
    bool commitPetDay(const int16_t *customStats, size_t customStatCount);
    bool findEvolutionTarget(AppearanceSelection &selection) const;
    bool applyEvolutionTarget();
    bool applyAppearance(const char *speciesCode, const char *outfitCode);

    int16_t customStat(uint8_t index) const;
    bool setCustomStat(uint8_t index, int16_t value);
    bool changeCustomStat(uint8_t index, int16_t delta);
    bool changeCustomStatClamped(uint8_t index, int16_t delta, int16_t minValue, int16_t maxValue);

    bool isFirstLaunchComplete() const;
    void markFirstLaunchComplete();
    void resetFirstLaunch();
    bool isFirstStartCompleted() const;
    void markFirstStartCompleted();
    void resetFirstStartCompleted();

    const char *speciesCode() const;
    const char *outfitCode() const;
    PetStatSnapshot statSnapshot() const;

private:
    static constexpr uint8_t savePeriodTicks = 2;
    Pet &pet;
    PetStorage &petStorage;
    Renderer &renderer;
    AppearanceLoader &appearanceLoader;
    uint8_t saveCounter = 0;
};

#endif // PET_ACTION_CONTROLLER_H
