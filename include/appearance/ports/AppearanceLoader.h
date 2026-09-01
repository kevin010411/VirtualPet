#ifndef APPEARANCE_LOADER_H
#define APPEARANCE_LOADER_H

#include <Arduino.h>
#include <stddef.h>
#include "pet/domain/Pet.h"
#include "shared/assets/AssetRuntimeContract.h"

struct PetBehaviorConfig;

struct AppearanceSelection
{
    uint8_t speciesSlot;
    uint8_t outfitSlot;
    AssetData::AnimationRef evolutionAnimation;
};

struct OutfitPreview
{
    uint8_t speciesSlot;
    uint8_t outfitSlot;
    AssetData::AnimationRef animation;
    uint16_t frameCount;
    uint16_t frameIntervalMs;
};

class AppearanceLoader
{
public:
    virtual ~AppearanceLoader() = default;
    virtual void configureRuntimeContract(const PetBehaviorConfig &) {}
    virtual bool validateRuntimeContracts(const PetStatSnapshot &stats) = 0;
    virtual bool lastContractLoadSucceeded() const = 0;
    virtual const char *firstAssetDataErrorResource() const = 0;
    virtual bool findInitialAppearance(AppearanceSelection &selection) = 0;
    virtual bool findEvolutionTarget(const PetStatSnapshot &stats, AppearanceSelection &selection) = 0;
    virtual bool loadSpecies(uint8_t *species, size_t maxSpecies, size_t &speciesCount) = 0;
    virtual bool loadOutfits(uint8_t speciesSlot, uint8_t unlockMask, uint8_t *outfits, size_t maxOutfits, size_t &outfitCount) = 0;
    virtual bool findOutfitPreview(uint8_t speciesSlot, uint8_t outfitSlot, bool locked, OutfitPreview &preview) = 0;
    virtual bool resolveOutfitUnlockMask(uint8_t speciesSlot, uint32_t stageDays,
                                         uint8_t currentMask, bool initialize,
                                         uint8_t &resolvedMask) = 0;
};

#endif // APPEARANCE_LOADER_H
