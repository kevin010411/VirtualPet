#ifndef PET_SESSION_H
#define PET_SESSION_H

#include "appearance/AppearanceChangeController.h"
#include <SdFat.h>

class Pet;
class PetStorage;
class Renderer;
class AppearanceLoader;
class PetBehaviorRuntime;
struct PetBehaviorConfig;

// Game activates the loaded contract in its presentation/command modules.
class PetSessionHost : public AppearanceChangeHost
{
public:
    virtual bool activateLoadedAppearance(uint8_t speciesSlot, uint8_t outfitSlot) = 0;
protected:
    ~PetSessionHost() = default;
};

// Owns the policy for producing a runnable pet; no input, layout or playback.
class PetSession
{
public:
    // Outcome of prepare(), consumed after platform/display initialization.
    // reset() reports each attempt, including required configuration failure,
    // through its return value so Game applies fatal policy without a callback.
    enum class State : uint8_t
    {
        NotPrepared, Ready, ContractFailed, ActiveAppearanceFailed,
        PetStateFailed, RestoredAppearanceFailed, UnlockFailed,
    };

    PetSession(Pet &pet, PetStorage &storage, Renderer &renderer,
               AppearanceLoader &loader, PetBehaviorConfig &config,
               PetBehaviorRuntime &runtime, AppearanceChangeController &changes,
               PetSessionHost &host);

    bool prepare(SdFat *sd);
    // Reset uses the validated startup selection and reloads its
    // contract before initializing unlocks/saving. Failure keeps prior mutations.
    AppearanceChangeResult reset();
    State state() const { return preparationState; }
    const char *errorStage() const;

private:
    enum class InitialState : uint8_t { Failed, Fresh, Restored };
    Pet &pet;
    PetStorage &storage;
    Renderer &renderer;
    AppearanceLoader &loader;
    PetBehaviorConfig &config;
    PetBehaviorRuntime &runtime;
    AppearanceChangeController &changes;
    PetSessionHost &host;
    State preparationState = State::NotPrepared;
    uint8_t initialSpeciesSlot = 0;
    uint8_t initialOutfitSlot = 0;

    InitialState loadPetState(bool allowSavedState);
};

#endif
