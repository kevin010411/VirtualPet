#ifndef APPEARANCE_CHANGE_CONTROLLER_H
#define APPEARANCE_CHANGE_CONTROLLER_H

#include <stdint.h>

class Pet;
class PetSaveController;
class PetStorage;
class Renderer;
class AppearanceLoader;

// Game distributes the loaded contract to its playback, commands and layout.
class AppearanceChangeHost
{
public:
    virtual bool configureActiveAppearance(uint8_t speciesSlot, uint8_t outfitSlot) = 0;
protected:
    ~AppearanceChangeHost() = default;
};

enum class AppearanceChangeResult : uint8_t
{
    Applied,
    ConfigurationFailed,
    PetStateRejected,
    UnlockFailed,
    SaveFailed,
};

class AppearanceChangeController
{
public:
    enum class ContractState : uint8_t { Reload, AlreadyActivated };

    AppearanceChangeController(Pet &pet, PetSaveController &saves, PetStorage &storage,
                               Renderer &renderer, AppearanceLoader &loader,
                               AppearanceChangeHost &host);

    // Rejection/save failure retains every mutation already made, including
    // contract activation before Pet mutation. Never roll back a failed save.
    AppearanceChangeResult changeSpecies(uint8_t speciesSlot, uint8_t entryOutfitSlot,
                                        ContractState contract = ContractState::Reload);
    AppearanceChangeResult applyOutfit(uint8_t outfitSlot, bool requiresUnlock);
    bool refreshUnlockState(bool initialize);

private:
    Pet &pet;
    PetSaveController &saves;
    PetStorage &storage;
    Renderer &renderer;
    AppearanceLoader &loader;
    AppearanceChangeHost &host;

    bool resolveUnlockState(bool initialize);
};

#endif
