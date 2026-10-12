#include "controller/Game.h"

#include "animation/AnimationController.h"
#include "controller/CommandController.h"
#include "display/LayoutRenderer.h"
#include "controller/MinigameController.h"
#if ENABLE_APPEARANCE_SELECTION
#include "appearance/AppearanceSelectionController.h"
#endif
#include "pet/PetBehaviorRuntime.h"
#include "pet/Pet.h"
#include "display/Renderer.h"
#include "common/AppProfile.h"
#if ENABLE_COLLECTION_CATALOG
#include "collection/CollectionCatalog.h"
#endif

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
    if (runtimeLoadState == RuntimeLoadState::Failed ||
        !petSession.prepare(animations->sdCard()))
    {
        enterFatalState();
        return false;
    }
    commands->resetSelection();
#if ENABLE_COLLECTION_CATALOG
    collectionCatalog->exit();
#endif
    layout->begin();

    dirtySelect = true;
    evolution->cancel();
    pendingFirstStartCompletion = false;
    clockStarted = false;
#if ENABLE_APPEARANCE_SELECTION
    appearanceSelection->exit();
#endif
#if ENABLE_GUESS_GAME
    minigame->reset();
#endif

    animations->setup(petBehaviorRuntime->baseAnimation());
    deathPresented = false;

    return true;
}

bool Game::finish_setup_game()
{
    const PetSession::State preparationState = petSession.state();
    const bool runtimeFailed = preparationState != PetSession::State::Ready &&
        preparationState != PetSession::State::NotPrepared &&
        preparationState != PetSession::State::PetStateFailed;
    if (preparationState != PetSession::State::Ready)
    {
        enterFatalState();
        const char *resource = "startup";
        if (runtimeFailed)
            resource = "runtime.bin";
        else if (preparationState == PetSession::State::PetStateFailed)
            resource = "pet state";
#if ENABLE_DEBUG
        renderer.showStartupResourceError(
            resource, preparationState == PetSession::State::NotPrepared
                          ? "prepare game" : petSession.errorStage());
#else
        // Only runtime preparation failures carry a more specific SD resource.
        // Preserve the distinct pet-state and unprepared diagnostics.
        if (runtimeFailed)
        {
            const char *recordedResource = renderer.firstAssetDataErrorResource();
            if (recordedResource != nullptr && recordedResource[0] != '\0')
                resource = recordedResource;
        }
        renderer.showResourceError(resource);
#endif
        return false;
    }

    // Session readiness cannot recover a Game that failed after preparation.
    if (flow.isFatalError())
    {
        renderer.showResourceError();
        return false;
    }

    layout->setCommandsVisible(!pet.isDead());
    layout->updateValues(pet.statSnapshot());
    uint8_t initialLayoutId = 0;
    if (!renderer.currentLayoutId(initialLayoutId) ||
        !layout->updatePlayback(initialLayoutId))
    {
        enterRuntimeFatal();
        return false;
    }
    if (renderer.firstAssetDataError() != AssetData::BundleError::None)
    {
        enterRuntimeFatal();
        return false;
    }

    initialized = true;
    if (enterDeathIfNeeded()) return !flow.isFatalError();
    enterCommand();
    petClock.reset(latestRtcSeconds);
    clockStarted = true;
    return true;
}

bool Game::resetPet()
{
    if (runtimeLoadState != RuntimeLoadState::Ready || flow.isFatalError())
        return false;
    const bool wasDead = pet.isDead();
    initialized = false;
#if ENABLE_COLLECTION_CATALOG
    collectionCatalog->exit();
#endif
    cheatEvolutionPending = false;
    evolution->cancel();
    pendingFirstStartCompletion = false;
    const AppearanceChangeResult resetResult = petSession.reset();
    if (resetResult != AppearanceChangeResult::Applied)
    {
        if (resetResult == AppearanceChangeResult::ConfigurationFailed ||
            resetResult == AppearanceChangeResult::UnlockFailed)
            enterRuntimeFatal();
        else
        {
            initialized = true;
            if (wasDead)
            {
                refreshBaseAnimation();
                animations->requestFullRedraw();
                deathSaveRetryAt = millis();
            }
            renderer.showResourceError("pet state");
        }
        return false;
    }
    deathPresented = deathSavePending = false;
    petSaves.resetTransientState();
    commands->resetSelection();
    layout->setCommandsVisible(true);
#if ENABLE_APPEARANCE_SELECTION
    appearanceSelection->exit();
#endif
#if ENABLE_COLLECTION_CATALOG
    collectionSavePending = false;
#endif
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
    petClock.reset(latestRtcSeconds);
    clockStarted = true;
    if (ENABLE_STARTUP_ANIMATION)
        startStartupAnimation();
    else
        enterCommand();
    return initialized;
}
