#include "presentation/application/Game.h"

#include "animation/application/AnimationController.h"
#include "commands/application/CommandController.h"
#include "presentation/application/LayoutRenderer.h"
#include "minigames/guess_item/application/MinigameController.h"
#if ENABLE_APPEARANCE_SELECTION
#include "appearance/application/AppearanceSelectionController.h"
#endif
#include "pet/application/PetActionController.h"
#include "pet_behavior/application/PetBehaviorRuntime.h"
#include "pet/domain/Pet.h"
#include "presentation/adapters/rendering/Renderer.h"
#include "pet/adapters/PetStorage.h"
#include "shared/config/AppProfile.h"

bool Game::setup_game()
{
    prepare_game();
    return finish_setup_game();
}

bool Game::prepare_game()
{
    if (flow.isFatalError())
        return false;
    initialized = false;
    cheatEvolutionPending = false;
    if (preparationState == PreparationState::RuntimeFailed)
        return false;
    // Any early contract/activation exit is a runtime preparation failure.
    // Publish Ready only after restoring the pet and setting up playback.
    preparationState = PreparationState::RuntimeFailed;
#if ENABLE_DEBUG
    startupDebugStage = nullptr;
#endif
    if (runtimeLoadState == RuntimeLoadState::Failed)
    {
#if ENABLE_DEBUG
        startupDebugStage = "runtime contract";
#endif
        return false;
    }

    AppearanceSelection initialAppearance = {};
    bool initialAppearanceResolved = false;
    char errorResource[20] = {};
    if (!loadInitialRuntimeContract(animations->sdCard(), initialAppearance,
                                    petBehaviorConfig, initialAppearanceResolved,
                                    errorResource, sizeof(errorResource)))
    {
        runtimeLoadState = RuntimeLoadState::Failed;
        if (initialAppearanceResolved)
        {
            renderer.recordAssetDataErrorResource(errorResource);
            flow.enterFatalError();
        }
#if ENABLE_DEBUG
        startupDebugStage = initialAppearanceResolved ? "active appearance" : "runtime contract";
#endif
        return false;
    }
    initialSpeciesSlot = initialAppearance.speciesSlot;
    initialOutfitSlot = initialAppearance.outfitSlot;
    if (!activateLoadedAppearance(initialAppearance.speciesSlot, initialAppearance.outfitSlot))
    {
        runtimeLoadState = RuntimeLoadState::Failed;
#if ENABLE_DEBUG
        startupDebugStage = "active appearance";
#endif
        return false;
    }
    commands->resetSelection();
    layout->begin();

    dirtySelect = true;
    clearPendingEvolution();
    pendingFirstStartCompletion = false;
    last_tick_time = millis();
#if ENABLE_APPEARANCE_SELECTION
    appearanceSelection->exit();
#endif
#if ENABLE_GUESS_GAME
    minigame->reset();
#endif

    const InitialPetStateResult initialState = loadInitialPetState(true, false);
    if (initialState == InitialPetStateResult::Failed)
    {
        preparationState = PreparationState::PetStateFailed;
#if ENABLE_DEBUG
        startupDebugStage = "pet state restore";
#endif
        return false;
    }

    // The initial contract already validated the active asset references.
    // A first-launch state normally selects that same appearance, so do not
    // reopen and revalidate the complete SD table a second time.  Reload only
    // when a compatible saved state actually restored a different appearance.
    const uint8_t restoredSpeciesSlot = pet.speciesSlot();
    const uint8_t restoredOutfitSlot = pet.outfitSlot();
    const bool appearanceChanged =
        restoredSpeciesSlot != petBehaviorConfig.activeSpeciesSlot ||
        restoredOutfitSlot != petBehaviorConfig.activeOutfitSlot;
    if (initialState == InitialPetStateResult::Restored && appearanceChanged &&
        !configureActiveAppearance(restoredSpeciesSlot, restoredOutfitSlot))
    {
        runtimeLoadState = RuntimeLoadState::Failed;
#if ENABLE_DEBUG
        startupDebugStage = "restored appearance";
#endif
        return false;
    }
    // Fresh state uses the initial contract already activated above. Restored
    // state keeps its saved appearance and only refreshes the unlock mask.
    const bool unlockStateReady = initialState == InitialPetStateResult::Restored
                                      ? refreshOutfitUnlockMask(false)
                                      : commitSpeciesAppearance(restoredSpeciesSlot, restoredOutfitSlot);
    if (!unlockStateReady)
    {
        runtimeLoadState = RuntimeLoadState::Failed;
#if ENABLE_DEBUG
        startupDebugStage = "outfit unlocks";
#endif
        return false;
    }

    // A successfully restored appearance is authoritative. Re-evaluating the
    // evolution table here can immediately replace a saved later-stage species
    // with an earlier wildcard/fallback match before the startup animation.
    // Evolution is still evaluated by handleEvolution() during normal ticks.
    animations->setup(petBehaviorRuntime->baseAnimation());

    preparationState = PreparationState::Ready;
    return true;
}

bool Game::finish_setup_game()
{
    if (preparationState != PreparationState::Ready)
    {
        flow.enterFatalError();
        const char *resource = "startup";
        if (preparationState == PreparationState::RuntimeFailed)
            resource = "runtime.bin";
        else if (preparationState == PreparationState::PetStateFailed)
            resource = "pet state";
#if ENABLE_DEBUG
        renderer.showStartupResourceError(
            resource, preparationState == PreparationState::NotPrepared
                          ? "prepare game" : startupDebugStage);
#else
        // Only runtime preparation failures carry a more specific SD resource.
        // Preserve the distinct pet-state and unprepared diagnostics.
        if (preparationState == PreparationState::RuntimeFailed)
        {
            const char *recordedResource = renderer.firstAssetDataErrorResource();
            if (recordedResource != nullptr && recordedResource[0] != '\0')
                resource = recordedResource;
        }
        renderer.showResourceError(resource);
#endif
        return false;
    }

    layout->updateValues(pet.statSnapshot());
    uint8_t initialLayoutId = 0;
    if (!renderer.currentLayoutId(initialLayoutId) ||
        !layout->updatePlayback(initialLayoutId))
    {
        flow.enterFatalError();
        renderer.showResourceError();
        return false;
    }
    if (renderer.firstAssetDataError() != AssetData::BundleError::None)
    {
        flow.enterFatalError();
        renderer.showResourceError();
        return false;
    }

    initialized = true;
    enterCommand();
    return true;
}

bool Game::resetPet()
{
    if (runtimeLoadState != RuntimeLoadState::Ready || flow.isFatalError())
        return false;
    initialized = false;
    cheatEvolutionPending = false;
    if (loadInitialPetState(false) == InitialPetStateResult::Failed)
        return false;

    clearPendingEvolution();
    pendingFirstStartCompletion = false;
    pet.resetFirstStartCompleted();
    if (!enterSpecies(pet.speciesSlot(), pet.outfitSlot()))
        return false;
    animations->cancelAll();
#if ENABLE_GUESS_GAME
    minigame->reset();
#endif
    refreshBaseAnimation();
    animations->requestFullRedraw();
    // startStartupAnimation() deliberately rejects calls before the game is
    // ready.  A left+right reset must therefore re-enable the game before it
    // queues FirstStart.
    initialized = true;
    if (ENABLE_STARTUP_ANIMATION)
        startStartupAnimation();
    else if (isFirstLaunchSelectionPending())
        enterFirstLaunch();
    else
        enterCommand();
    return true;
}

Game::InitialPetStateResult Game::loadInitialPetState(bool allowSavedState,
                                                       bool showError)
{
    if (allowSavedState && petStorage.load(pet, petBehaviorConfig.schemaFingerprint))
    {
        OutfitPreview preview = {};
        if (pet.speciesSlot() != 0 && pet.outfitSlot() != 0 &&
            appearanceLoader.findOutfitPreview(
                pet.speciesSlot(), pet.outfitSlot(), false, preview))
        {
            return InitialPetStateResult::Restored;
        }
        petStorage.discard();
    }

    pet.setDefaultState();
    pet.setSchemaFingerprint(petBehaviorConfig.schemaFingerprint);
    petBehaviorRuntime->initializeStats();
    const bool applied = pet.setSpeciesSlot(initialSpeciesSlot) &&
                         pet.setOutfitSlot(initialOutfitSlot);
    if (!applied && showError)
        renderer.showResourceError();
    return applied ? InitialPetStateResult::Fresh : InitialPetStateResult::Failed;
}
