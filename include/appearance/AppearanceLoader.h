#ifndef APPEARANCE_LOADER_H
#define APPEARANCE_LOADER_H

#include <Arduino.h>
#include <stddef.h>
#include "pet/Pet.h"
#include "resources/AssetRuntimeContract.h"

struct PetBehaviorConfig;

enum class EvolutionAnimationMode : uint8_t
{
    Disabled = 0,
    Single = 1,
    TwoPhase = 2,
};

enum class EvolutionLookupResult : uint8_t
{
    Found,
    NoTarget,
    LoadFailed,
};

struct AppearanceSelection
{
    uint8_t speciesSlot;
    uint8_t outfitSlot;
    EvolutionAnimationMode evolutionMode;
    AssetData::AnimationRef sourceEvolutionAnimation;
    AssetData::AnimationRef targetEvolutionAnimation;
    uint8_t sourceEvolutionPlaybackCount;
    uint8_t targetEvolutionPlaybackCount;
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
    virtual void configureRuntimeContract(const PetBehaviorConfig &config) = 0;
    virtual const char *firstAssetDataErrorResource() const = 0;
    virtual EvolutionLookupResult findEvolutionTarget(const PetStatSnapshot &stats,
                                                      AppearanceSelection &selection) = 0;
    virtual bool loadSpecies(uint8_t *species, size_t maxSpecies, size_t &speciesCount) = 0;
    virtual bool loadOutfits(uint8_t speciesSlot, uint8_t unlockMask, uint8_t *outfits, size_t maxOutfits, size_t &outfitCount) = 0;
    virtual bool findOutfitPreview(uint8_t speciesSlot, uint8_t outfitSlot, bool locked, OutfitPreview &preview) = 0;
    virtual bool resolveOutfitUnlockMask(uint8_t speciesSlot, const PetStatSnapshot &stats,
                                         uint8_t currentMask, bool initialize,
                                         uint8_t &resolvedMask) = 0;
    virtual bool resolveConsumableOutfitUnlock(uint8_t speciesSlot, uint8_t outfitSlot,
                                                const PetStatSnapshot &stats,
                                                PetStatSnapshot &consumedStats) = 0;
};

#endif // APPEARANCE_LOADER_H
