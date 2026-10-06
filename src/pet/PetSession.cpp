#include "pet/PetSession.h"

#include "appearance/AppearanceLoader.h"
#include "display/Renderer.h"
#include "pet/Pet.h"
#include "pet/PetStorage.h"
#include "pet/PetBehaviorRuntime.h"
#include "resources/RuntimeContractLoader.h"

PetSession::PetSession(Pet &petRef, PetStorage &storageRef, Renderer &rendererRef,
                       AppearanceLoader &loaderRef, PetBehaviorConfig &configRef,
                       PetBehaviorRuntime &runtimeRef, AppearanceChangeController &changesRef,
                       PetSessionHost &hostRef)
    : pet(petRef), storage(storageRef), renderer(rendererRef), loader(loaderRef),
      config(configRef), runtime(runtimeRef), changes(changesRef), host(hostRef)
{
}

bool PetSession::prepare(SdFat *sd)
{
    // Runtime preparation failure is terminal until reboot. Pet-state failure
    // remains retryable, as in the existing two-stage startup flow.
    if (preparationState != State::NotPrepared && preparationState != State::Ready &&
        preparationState != State::PetStateFailed)
        return false;
    preparationState = State::ContractFailed;
    AppearanceSelection initial = {};
    bool resolved = false;
    char resource[20] = {};
    if (!loadInitialRuntimeContract(sd, initial, config, resolved, resource, sizeof(resource)))
    {
        if (resolved)
        {
            renderer.recordAssetDataErrorResource(resource);
            preparationState = State::ActiveAppearanceFailed;
        }
        return false;
    }
    initialSpeciesSlot = initial.speciesSlot;
    initialOutfitSlot = initial.outfitSlot;
    preparationState = State::ActiveAppearanceFailed;
    if (!host.activateLoadedAppearance(initialSpeciesSlot, initialOutfitSlot))
        return false;

    const InitialState initialState = loadPetState(true);
    if (initialState == InitialState::Failed)
    {
        preparationState = State::PetStateFailed;
        return false;
    }

    // Saved appearance is authoritative; do not evaluate evolution at startup.
    // Reuse the activated startup contract unless a compatible save differs.
    preparationState = State::RestoredAppearanceFailed;
    if (initialState == InitialState::Restored &&
        (pet.speciesSlot() != config.activeSpeciesSlot ||
         pet.outfitSlot() != config.activeOutfitSlot) &&
        !host.configureActiveAppearance(pet.speciesSlot(), pet.outfitSlot()))
        return false;

    preparationState = State::UnlockFailed;
    const bool ready = initialState == InitialState::Restored
        ? changes.refreshUnlockState(false)
        : changes.changeSpecies(pet.speciesSlot(), pet.outfitSlot(),
              AppearanceChangeController::ContractState::AlreadyActivated) == AppearanceChangeResult::Applied;
    if (!ready)
        return false;
    preparationState = State::Ready;
    return true;
}

AppearanceChangeResult PetSession::reset()
{
    if (preparationState != State::Ready)
        return AppearanceChangeResult::PetStateRejected;
    if (loadPetState(false) == InitialState::Failed)
    {
        return AppearanceChangeResult::PetStateRejected;
    }
    pet.resetFirstStartCompleted();
    return changes.changeSpecies(pet.speciesSlot(), pet.outfitSlot());
}

PetSession::InitialState PetSession::loadPetState(bool allowSavedState)
{
    if (allowSavedState && storage.load(pet, config.schemaFingerprint))
    {
        OutfitPreview preview = {};
        if (pet.speciesSlot() != 0 && pet.outfitSlot() != 0 &&
            loader.findOutfitPreview(pet.speciesSlot(), pet.outfitSlot(), false, preview))
            return InitialState::Restored;
        storage.discard();
    }
    pet.setDefaultState();
    pet.setSchemaFingerprint(config.schemaFingerprint);
    runtime.initializeStats();
    return pet.setSpeciesSlot(initialSpeciesSlot) && pet.setOutfitSlot(initialOutfitSlot)
        ? InitialState::Fresh : InitialState::Failed;
}

const char *PetSession::errorStage() const
{
    switch (preparationState)
    {
    case State::ContractFailed: return "runtime contract";
    case State::ActiveAppearanceFailed: return "active appearance";
    case State::PetStateFailed: return "pet state restore";
    case State::RestoredAppearanceFailed: return "restored appearance";
    case State::UnlockFailed: return "outfit unlocks";
    default: return nullptr;
    }
}
