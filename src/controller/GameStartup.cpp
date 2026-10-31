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
    if (flow.is(AppStage::FatalError))
        return false;
    initialized = false;
    cheatEvolutionPending = false;
    if (runtimeLoadState == RuntimeLoadState::Failed ||
        !petSession.prepare(animations->sdCard()))
    {
        enterFatalState();
        return false;
    }
    cancelForeground();
    commands->resetSelection();
    layout->begin();
    dirtySelect = true;
    pendingFirstStartCompletion = false;
    clockStarted = false;

    animations->setup(petBehaviorRuntime->baseAnimation());
    refreshBaseAnimation();
#if ENABLE_DEATH
    deathPresented = false;
#endif

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
    if (flow.is(AppStage::FatalError))
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
    if (enterDeathIfNeeded()) return !flow.is(AppStage::FatalError);
    enterCommand();
    petClock.reset(latestRtcSeconds);
    clockStarted = true;
    return true;
}

bool Game::resetPet()
{
    if (runtimeLoadState != RuntimeLoadState::Ready || flow.is(AppStage::FatalError))
        return false;
    const bool wasDead = pet.isDead();
    initialized = false;
#if ENABLE_COLLECTION_CATALOG
    collectionCatalog->exit();
    if (flow.is(AppStage::Collection)) enterCommand();
#endif
    cheatEvolutionPending = false;
    evolution->cancel();
    flow.endEvolution();
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
#if ENABLE_DEATH
                deathSaveRetryAt = millis();
#endif
            }
            renderer.showResourceError("pet state");
        }
        return false;
    }
#if ENABLE_DEATH
    deathPresented = deathSavePending = false;
#endif
    petSaves.resetTransientState();
    commands->resetSelection();
    layout->setCommandsVisible(true);
#if ENABLE_COLLECTION_CATALOG
    collectionSavePending = false;
#endif
    cancelForeground();
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
