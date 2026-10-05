#include "appearance/AppearanceChangeController.h"

#include "appearance/AppearanceLoader.h"
#include "pet/Pet.h"
#include "pet/PetSaveController.h"
#include "pet/PetStorage.h"
#include "display/Renderer.h"

AppearanceChangeController::AppearanceChangeController(
    Pet &petRef, PetSaveController &savesRef, PetStorage &storageRef,
    Renderer &rendererRef, AppearanceLoader &loaderRef, AppearanceChangeHost &hostRef)
    : pet(petRef), saves(savesRef), storage(storageRef), renderer(rendererRef),
      loader(loaderRef), host(hostRef)
{
}

AppearanceChangeResult AppearanceChangeController::changeSpecies(
    uint8_t speciesSlot, uint8_t entryOutfitSlot, ContractState contract)
{
    if (contract == ContractState::Reload &&
        !host.configureActiveAppearance(speciesSlot, entryOutfitSlot))
        return AppearanceChangeResult::ConfigurationFailed;
    if (!pet.setSpeciesSlot(speciesSlot) || !pet.setOutfitSlot(entryOutfitSlot))
    {
        renderer.recordAssetDataErrorResource("pet appearance");
        return AppearanceChangeResult::PetStateRejected;
    }
    if (!resolveUnlockState(true))
    {
        renderer.recordAssetDataErrorResource(loader.firstAssetDataErrorResource());
        return AppearanceChangeResult::UnlockFailed;
    }
    if (!saves.saveNow())
    {
        renderer.recordAssetDataErrorResource(
            storage.lastSaveSlot() == 'B' ? "state_b.bin" : "state_a.bin");
        return AppearanceChangeResult::SaveFailed;
    }
    return AppearanceChangeResult::Applied;
}

AppearanceChangeResult AppearanceChangeController::applyOutfit(
    uint8_t outfitSlot, bool requiresUnlock)
{
    if (!host.configureActiveAppearance(pet.speciesSlot(), outfitSlot))
        return AppearanceChangeResult::ConfigurationFailed;
    if (requiresUnlock)
    {
        PetStatSnapshot consumedStats = {};
        if (!loader.resolveConsumableOutfitUnlock(
                pet.speciesSlot(), outfitSlot, pet.statSnapshot(), consumedStats))
            return AppearanceChangeResult::UnlockFailed;
        if (!pet.commitConsumableOutfitUnlock(outfitSlot, consumedStats))
            return AppearanceChangeResult::PetStateRejected;
    }
    else if (!pet.setSpeciesSlot(pet.speciesSlot()) || !pet.setOutfitSlot(outfitSlot))
        return AppearanceChangeResult::PetStateRejected;
    renderer.setAssetAppearance(pet.speciesSlot(), pet.outfitSlot());
    return saves.saveNow() ? AppearanceChangeResult::Applied : AppearanceChangeResult::SaveFailed;
}

bool AppearanceChangeController::resolveUnlockState(bool initialize)
{
    uint8_t mask = 0;
    if (!loader.resolveOutfitUnlockMask(
            pet.speciesSlot(), pet.statSnapshot(), pet.outfitUnlockMask(), initialize, mask))
        return false;
    pet.initializeOutfitUnlockMask(mask);
    return true;
}

bool AppearanceChangeController::refreshUnlockState(bool initialize)
{
    const uint8_t previousMask = pet.outfitUnlockMask();
    return resolveUnlockState(initialize) &&
           (pet.outfitUnlockMask() == previousMask || saves.saveNow());
}
