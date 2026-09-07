#include "presentation/application/Game.h"

#include "animation/application/AnimationController.h"
#include "commands/application/CommandController.h"
#include "commands/application/CommandExecutor.h"
#include "presentation/application/LayoutRenderer.h"
#include "minigames/guess_item/application/MinigameController.h"
#if ENABLE_APPEARANCE_SELECTION
#include "appearance/application/AppearanceSelectionController.h"
#endif
#include "pet/application/PetActionController.h"
#include "pet_behavior/application/PetBehaviorRuntime.h"
#include "pet_behavior/domain/RuntimeTableBehavior.h"
#include "pet/domain/Pet.h"
#include "presentation/adapters/rendering/Renderer.h"
#include "pet/adapters/PetStorage.h"
#include "shared/config/AppProfile.h"

Game::Game(Pet &petRef, PetStorage &petStorageRef, Renderer &rendererRef, AppearanceLoader &appearanceLoaderRef)
    : pet(petRef),
      petStorage(petStorageRef),
      renderer(rendererRef),
      appearanceLoader(appearanceLoaderRef),
      petActions(std::make_unique<PetActionController>(pet, petStorage, renderer, appearanceLoader)),
      animations(std::make_unique<AnimationController>(renderer)),
      petBehaviorRuntime(std::make_unique<PetBehaviorRuntime>(petBehaviorConfig, *petActions, *animations, renderer)),
      commandExecutor(std::make_unique<CommandExecutor>(*petActions, *animations)),
      commands(std::make_unique<CommandController>(*commandExecutor)),
      layout(std::make_unique<LayoutRenderer>(renderer, *commands))
#if ENABLE_APPEARANCE_SELECTION
      ,
      appearanceSelection(std::make_unique<AppearanceSelectionController>(renderer, appearanceLoader))
#endif
#if ENABLE_GUESS_GAME
      ,
      minigame(std::make_unique<MinigameController>(*animations, *petBehaviorRuntime))
#endif
{
}

Game::~Game()
    = default;

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
    setupPrepared = false;
    initialStateLoadingFailed = false;
    if (startupConfigError != nullptr)
        return false;
    AssetData::RuntimeManifest manifest = {};
    if (petBehaviorLoadingFailed ||
        !loadRuntimeManifest(animations->sdCard(), manifest))
    {
        petBehaviorLoadingFailed = true;
        petBehaviorLoaded = false;
        startupConfigError = "runtime.bin";
        return false;
    }

    petBehaviorConfig = {};
    petBehaviorConfig.assetManifest = manifest;
    appearanceLoader.configureRuntimeContract(petBehaviorConfig);
    AppearanceSelection initialAppearance = {};
    if (!appearanceLoader.findInitialAppearance(initialAppearance) ||
        !configureActiveAppearance(initialAppearance.speciesSlot, initialAppearance.outfitSlot))
    {
        petBehaviorLoadingFailed = true;
        petBehaviorLoaded = false;
        startupConfigError = "runtime.bin";
        return false;
    }
    commands->resetSelection();
    layout->begin();

    dirtySelect = true;
    pendingEvolution = false;
    pendingFirstStartCompletion = false;
    pendingEvolutionSpeciesSlot = 0;
    pendingEvolutionOutfitSlot = 0;
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
        initialStateLoadingFailed = true;
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
        petBehaviorLoadingFailed = true;
        petBehaviorLoaded = false;
        startupConfigError = "runtime.bin";
        return false;
    }
    const bool unlockStateReady = initialState == InitialPetStateResult::Restored
                                      ? refreshOutfitUnlockMask(false)
                                      : enterSpecies(restoredSpeciesSlot, restoredOutfitSlot);
    if (!unlockStateReady)
    {
        petBehaviorLoadingFailed = true;
        petBehaviorLoaded = false;
        startupConfigError = "runtime.bin";
        return false;
    }

    // A successfully restored appearance is authoritative. Re-evaluating the
    // evolution table here can immediately replace a saved later-stage species
    // with an earlier wildcard/fallback match before the startup animation.
    // Evolution is still evaluated by handleEvolution() during normal ticks.
    animations->setup(petBehaviorConfig.idleAnimation);

    setupPrepared = true;
    return true;
}

bool Game::finish_setup_game()
{
    if (!setupPrepared)
    {
        flow.enterFatalError();
        if (startupConfigError != nullptr)
            renderer.showResourceError();
        else if (initialStateLoadingFailed)
            renderer.showResourceError();
        return false;
    }

    layout->drawAll();
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

void Game::loop_game()
{
    if (!initialized)
        return;

    if (renderer.firstAssetDataError() != AssetData::BundleError::None)
        flow.enterFatalError();

    if (flow.isFatalError())
    {
        renderer.showResourceError();
        return;
    }

    const unsigned long now = millis();
    const unsigned long elapsed = now - last_tick_time;

    if (elapsed >= gameTick)
    {
        last_tick_time = now;

        if (flow.isCommand() || flow.isMinigame())
            maybeTickPet();

        if (flow.isStartup() && !animations->isBusy())
        {
            if (isFirstLaunchSelectionPending())
                enterFirstLaunch();
            else
                enterCommand();
        }
    }

#if ENABLE_GUESS_GAME
    if (flow.isMinigame())
    {
        minigame->update();
        if (!minigame->isActive())
        {
            if (!refreshOutfitUnlockMask(false))
                renderer.showResourceError();
            flow.onMinigameEnded();
        }
    }
#endif

#if ENABLE_APPEARANCE_SELECTION
    if (appearanceSelection->isActive())
    {
        appearanceSelection->render(now);
        if (dirtySelect)
        {
            layout->drawSelection();
            dirtySelect = false;
        }
#if ENABLE_DEBUG
        renderer.renderDebugOverlay();
#endif
        return;
    }
#endif

    const PlaybackTickResult playbackResult = animations->tick(now);
    handlePlaybackResult(playbackResult.result);
    completeFirstStartIfReady(playbackResult);

    if (flow.isFatalError())
    {
        renderer.showResourceError();
        return;
    }

    if (layout->isActionActive() && (!flow.isCommand() || !animations->isBusy()))
    {
        refreshBaseAnimation();
        layout->endAction();
    }
    syncActionLayoutWithPlayback();

    if (dirtySelect)
    {
        layout->drawSelection();
        dirtySelect = false;
    }
#if ENABLE_DEBUG
    renderer.renderDebugOverlay();
#endif
}

void Game::requestFullRedraw()
{
    dirtySelect = true;
    animations->requestFullRedraw();
}

void Game::redrawAllNow()
{
    if (!initialized)
        return;

    const unsigned long now = millis();

    // Repaint the entire center area even when an animation frame was already
    // considered current before STOP mode.
    animations->requestFullRedraw();
    const PlaybackTickResult playbackResult = animations->tick(now);
    handlePlaybackResult(playbackResult.result);
    completeFirstStartIfReady(playbackResult);

#if ENABLE_APPEARANCE_SELECTION
    if (appearanceSelection->isActive())
    {
        appearanceSelection->requestFullRedraw();
        appearanceSelection->render(now);
    }
#endif

    // drawSelection() only updates two slots. After display sleep the complete
    // top and bottom layout must be restored from the SD card.
    layout->drawAll();
    dirtySelect = false;

#if ENABLE_DEBUG
    renderer.renderDebugOverlay();
#endif
}

void Game::setRendererAssetAppearance(uint8_t speciesSlot, uint8_t outfitSlot)
{
    if (petBehaviorLoadingFailed)
    {
        renderer.showResourceError();
        return;
    }
    if (!configureActiveAppearance(speciesSlot, outfitSlot) ||
        !pet.setSpeciesSlot(speciesSlot) || !pet.setOutfitSlot(outfitSlot))
    {
        initialized = false;
        renderer.showResourceError();
        return;
    }

    refreshBaseAnimation();
}

bool Game::configureActiveAppearance(uint8_t speciesSlot, uint8_t outfitSlot)
{
    char errorResource[20] = {};
    if (!loadRuntimeContract(animations->sdCard(), speciesSlot, outfitSlot,
                             petBehaviorConfig, errorResource, sizeof(errorResource)))
    {
        renderer.recordAssetDataErrorResource(errorResource);
        petBehaviorLoaded = false;
        petBehaviorLoadingFailed = true;
        flow.enterFatalError();
        return false;
    }
    if (!renderer.configureAssetBundle(petBehaviorConfig.assetManifest.bundleId))
    {
        petBehaviorLoaded = false;
        petBehaviorLoadingFailed = true;
        flow.enterFatalError();
        return false;
    }

    renderer.setAssetAppearance(speciesSlot, outfitSlot);
    appearanceLoader.configureRuntimeContract(petBehaviorConfig);
    if (!appearanceLoader.validateRuntimeContracts(pet.statSnapshot()))
    {
        renderer.recordAssetDataErrorResource(appearanceLoader.firstAssetDataErrorResource());
        petBehaviorLoaded = false;
        petBehaviorLoadingFailed = true;
        flow.enterFatalError();
        renderer.showResourceError(appearanceLoader.firstAssetDataErrorResource());
        return false;
    }
    animations->configureRuntimeContract(petBehaviorConfig);
    commandExecutor->configureRuntimeContract(petBehaviorConfig);
    commands->configure(petBehaviorConfig);
    layout->configureRuntimeContract(petBehaviorConfig);
    petBehaviorLoaded = true;
    return true;
}

bool Game::resolveOutfitUnlockMask(bool initialize)
{
    uint8_t mask = 0;
    if (!appearanceLoader.resolveOutfitUnlockMask(
            pet.speciesSlot(), pet.statSnapshot(), pet.outfitUnlockMask(), initialize, mask))
        return false;
    pet.initializeOutfitUnlockMask(mask);
    return true;
}

bool Game::refreshOutfitUnlockMask(bool initialize)
{
    const uint8_t previousMask = pet.outfitUnlockMask();
    return resolveOutfitUnlockMask(initialize) &&
           (pet.outfitUnlockMask() == previousMask || petActions->saveNow());
}

bool Game::enterSpecies(uint8_t speciesSlot, uint8_t entryOutfitSlot)
{
    if (!configureActiveAppearance(speciesSlot, entryOutfitSlot) ||
        !petActions->stageAppearance(speciesSlot, entryOutfitSlot) ||
        !resolveOutfitUnlockMask(true))
        return false;
    return petActions->saveNow();
}

bool Game::saveNow()
{
    return initialized && petActions->saveNow();
}

bool Game::startStartupAnimation()
{
    if (!initialized)
        return false;
    if (!flow.requestStartup())
        return false;

    return beginStartupAnimation();
}

bool Game::hasTransientAnimation() const
{
    return animations->isBusy();
}

void Game::startBatteryAnimation()
{
    if (!initialized)
        return;
    flow.enterBattery();
    animations->startBatteryAnimation();
}

void Game::endBatteryAnimation()
{
    if (!initialized)
        return;
    flow.leaveBattery();
    requestFullRedraw();
}

void Game::updateBatteryAnimation(unsigned long now)
{
    if (!initialized)
        return;
    animations->updateBatteryAnimation(now);
#if ENABLE_DEBUG
    renderer.renderDebugOverlay();
#endif
}

void Game::OnLeftKey()
{
    if (!initialized)
        return;
#if ENABLE_APPEARANCE_SELECTION
    if (appearanceSelection->isActive())
    {
        appearanceSelection->onLeft();
        return;
    }
#endif

    if (flow.isFirstLaunch())
    {
        return;
    }

#if ENABLE_GUESS_GAME
    if (flow.isMinigame())
    {
        minigame->onLeft();
        return;
    }
#endif

    if (flow.isCommand())
    {
        commands->prev();
        dirtySelect = true;
    }
}

void Game::OnRightKey()
{
    if (!initialized)
        return;
#if ENABLE_APPEARANCE_SELECTION
    if (appearanceSelection->isActive())
    {
        appearanceSelection->onRight();
        return;
    }
#endif

    if (flow.isFirstLaunch())
    {
        return;
    }

#if ENABLE_GUESS_GAME
    if (flow.isMinigame())
    {
        minigame->onRight();
        return;
    }
#endif

    if (flow.isCommand())
    {
        commands->next();
        dirtySelect = true;
    }
}

void Game::OnConfirmKey()
{
    if (!initialized)
        return;
#if ENABLE_APPEARANCE_SELECTION
    if (appearanceSelection->isActive())
    {
        if (appearanceSelection->isSelectingSpecies())
        {
            uint8_t selectedSpecies = 0;
            uint8_t selectedOutfit = 0;
            const bool confirmed = appearanceSelection->onConfirmSpecies(
                selectedSpecies, selectedOutfit);
            if (confirmed)
            {
                if (enterSpecies(selectedSpecies, selectedOutfit))
                {
                    refreshBaseAnimation();
                }
                else
                    renderer.showResourceError();
            }
            animations->requestFullRedraw();
            completeFirstLaunchIfNeeded(AppCommandId::ChangeSpecies);
            return;
        }

        uint8_t selectedOutfit = 0;
        bool requiresUnlock = false;
        const bool confirmed = appearanceSelection->onConfirm(selectedOutfit, requiresUnlock);
        if (confirmed)
        {
            if (configureActiveAppearance(petActions->speciesSlot(), selectedOutfit))
            {
                bool applied = false;
                if (requiresUnlock)
                {
                    PetStatSnapshot consumedStats = {};
                    if (appearanceLoader.resolveConsumableOutfitUnlock(
                            petActions->speciesSlot(), selectedOutfit,
                            petActions->statSnapshot(), consumedStats))
                        applied = petActions->applyConsumableOutfitUnlock(
                            selectedOutfit, consumedStats);
                }
                else
                    applied = petActions->applyAppearance(
                        petActions->speciesSlot(), selectedOutfit);
                if (applied)
                {
                    appearanceSelection->exit();
                    refreshBaseAnimation();
                }
            }
            else
                renderer.showResourceError();
        }
        animations->requestFullRedraw();
        completeFirstLaunchIfNeeded(AppCommandId::ChangeOutfit);
        return;
    }
#endif

#if ENABLE_GUESS_GAME
    if (flow.isMinigame())
    {
        minigame->onConfirm();
        return;
    }
#endif

    if (flow.isFirstLaunch())
    {
        startFirstLaunchRequiredCommand();
        return;
    }

    if (!flow.isFirstLaunch() && !flow.isCommand())
        return;

    const AppCommandId commandId = commands->currentCommandId();
    const int selectedSlot = commands->selectedSlot();
    const PetBehaviorButtonConfig &behaviorButton = petBehaviorConfig.buttons[selectedSlot];
    if (behaviorButton.active && behaviorButton.kind == PetBehaviorButtonKind::UserAction)
    {
        const PetBehaviorActionResult actionResult = petBehaviorRuntime->executeAction(behaviorButton.actionSlot);
        if (actionResult != PetBehaviorActionResult::Rejected)
        {
            if (actionResult == PetBehaviorActionResult::AppliedAnimationMissing)
                renderer.showResourceError();
            if (!refreshOutfitUnlockMask(false))
                renderer.showResourceError();
            refreshBaseAnimation();
            layout->enterAction(animations->currentPlaybackRole(), selectedSlot);
        }
        return;
    }
    commandExecutor->begin(commandId);
    const bool executed = commands->executeCurrent();
    handleCommandResult(commandExecutor->complete(executed), selectedSlot);
}

bool Game::resetPet()
{
    if (!petBehaviorLoaded || flow.isFatalError())
        return false;
    initialized = false;
    if (loadInitialPetState(false) == InitialPetStateResult::Failed)
        return false;

    pendingEvolution = false;
    pendingFirstStartCompletion = false;
    pendingEvolutionSpeciesSlot = 0;
    pendingEvolutionOutfitSlot = 0;
    petActions->resetFirstStartCompleted();
    if (!enterSpecies(petActions->speciesSlot(), petActions->outfitSlot()))
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

void Game::refreshBaseAnimation()
{
    animations->setBaseAnimation(petBehaviorRuntime->baseAnimation());
}

void Game::syncActionLayoutWithPlayback()
{
    if (flow.isCommand())
        layout->updateAction(animations->currentPlaybackRole());
}

void Game::handleCommandResult(const CommandResult &result, int selectedSlot)
{
    if (!result.executed)
        return;

    layout->enterAction(result.layoutPlaybackRole, selectedSlot);
    if (result.resourceError)
        renderer.showResourceError();

#if ENABLE_COMMAND_OUTFIT
    if (result.requestedOutfit)
    {
        if (appearanceSelection->start(petActions->speciesSlot(), petActions->outfitSlot(),
                                       pet.outfitUnlockMask()))
        {
            animations->cancelAll();
            animations->requestFullRedraw();
        }
        return;
    }
#endif

#if ENABLE_COMMAND_SPECIES
    if (result.requestedSpecies)
    {
        if (appearanceSelection->startSpecies(petActions->speciesSlot(), pet.statSnapshot()))
        {
            animations->cancelAll();
            animations->requestFullRedraw();
        }
        return;
    }
#endif

#if ENABLE_GUESS_GAME
    if (result.requestedMinigame && !flow.isFirstLaunch())
    {
        minigame->startGuessItem();
        flow.enterMinigame();
        return;
    }
#endif

    completeFirstLaunchIfNeeded(result.commandId);
}

void Game::completeFirstLaunchIfNeeded(AppCommandId commandId)
{
    if (!flow.isFirstLaunch())
        return;

    const bool shouldStart = flow.completeFirstLaunch(commandId);
    if (!flow.isFirstLaunch())
    {
        petActions->markFirstLaunchComplete();
        petActions->saveNow();
    }

    if (shouldStart)
        beginStartupAnimation();
    else if (!flow.isFirstLaunch())
        enterCommand();
}

bool Game::isFirstLaunchSelectionPending() const
{
    return ENABLE_APPEARANCE_SELECTION && ENABLE_FIRST_LAUNCH_SELECTION && !petActions->isFirstLaunchComplete();
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

    AppearanceSelection initialAppearance = {};
    if (!appearanceLoader.findInitialAppearance(initialAppearance))
    {
        if (showError)
            renderer.showResourceError();
        return InitialPetStateResult::Failed;
    }

    pet.setDefaultState();
    pet.setSchemaFingerprint(petBehaviorConfig.schemaFingerprint);
    petBehaviorRuntime->initializeStats();
    const bool applied = pet.setSpeciesSlot(initialAppearance.speciesSlot) &&
                         pet.setOutfitSlot(initialAppearance.outfitSlot);
    if (!applied && showError)
        renderer.showResourceError();
    return applied ? InitialPetStateResult::Fresh : InitialPetStateResult::Failed;
}

bool Game::startFirstLaunchRequiredCommand()
{
#if ENABLE_APPEARANCE_SELECTION
    switch (flow.firstLaunchRequiredCommand())
    {
    case AppCommandId::ChangeOutfit:
        if (appearanceSelection->start(petActions->speciesSlot(), petActions->outfitSlot(),
                                       pet.outfitUnlockMask()))
        {
            animations->cancelAll();
            animations->requestFullRedraw();
            return true;
        }
        break;
    default:
        break;
    }
#endif

    completeFirstLaunchIfNeeded(flow.firstLaunchRequiredCommand());
    return false;
}

void Game::maybeTickPet()
{
    if (completePendingEvolutionIfReady())
        return;

    if (pendingEvolution)
        return;

    if (!animations->isBusy())
    {
        handleEvolution();
        if (pendingEvolution)
            return;
    }

    if (!animations->isBusy())
    {
        if (!petBehaviorRuntime->advancePetDay())
            return;
        if (!refreshOutfitUnlockMask(false))
        {
            renderer.showResourceError();
            return;
        }
        handleEvolution();
        if (pendingEvolution)
            return;

    }

    refreshBaseAnimation();
    petActions->maybeSave();
}

bool Game::completePendingEvolutionIfReady()
{
    if (!pendingEvolution)
        return false;

    if (animations->isBusy())
        return false;

    if (!enterSpecies(pendingEvolutionSpeciesSlot, pendingEvolutionOutfitSlot))
    {
        renderer.showResourceError();
        return false;
    }
    pendingEvolution = false;
    pendingEvolutionSpeciesSlot = 0;
    pendingEvolutionOutfitSlot = 0;
    // Species entry changes the in-memory appearance before its single
    // persistence write. Always hand rendering back to the current base
    // animation so the display cannot remain on the completed Evolution frame.
    refreshBaseAnimation();
    animations->requestFullRedraw();
    return true;
}

void Game::handleEvolution()
{
    AppearanceSelection selection = {};
    if (!petActions->findEvolutionTarget(selection))
    {
        if (!appearanceLoader.lastContractLoadSucceeded())
        {
            petBehaviorLoaded = false;
            petBehaviorLoadingFailed = true;
            flow.enterFatalError();
            renderer.showResourceError(appearanceLoader.firstAssetDataErrorResource());
        }
        return;
    }

    if (!beginEvolutionAnimation(selection))
    {
        if (enterSpecies(selection.speciesSlot, selection.outfitSlot))
        {
            refreshBaseAnimation();
            animations->requestFullRedraw();
        }
        else
            renderer.showResourceError();
    }
}

bool Game::beginEvolutionAnimation(const AppearanceSelection &selection)
{
    if (!animations->hasAnimation(selection.evolutionAnimation))
        return false;

    pendingEvolutionSpeciesSlot = selection.speciesSlot;
    pendingEvolutionOutfitSlot = selection.outfitSlot;
    pendingEvolution = true;

    const Animation animation = Animation::complete(
        selection.evolutionAnimation, 2, FirmwarePlaybackRole::Evolution);
    const PlaybackResult replaceResult = animations->replace(AnimationSequence(&animation, 1));
    if (replaceResult != PlaybackResult::Accepted)
    {
        pendingEvolution = false;
        pendingEvolutionSpeciesSlot = 0;
        pendingEvolutionOutfitSlot = 0;
        return false;
    }
    return true;
}

bool Game::beginStartupAnimation()
{
#if ENABLE_STARTUP_ANIMATION
    const bool hasIntro = animations->hasAnimation(FirmwarePlaybackRole::StartIntro);
    const bool hasSpeciesStart = animations->hasAnimation(FirmwarePlaybackRole::Start);
    const bool needsFirstStart = ENABLE_FIRST_START_ANIMATION && !petActions->isFirstStartCompleted();
    const bool hasFirstStart = animations->hasAnimation(FirmwarePlaybackRole::FirstStart);
    if (needsFirstStart && !hasFirstStart)
    {
        flow.requestStartup();
        animations->cancelAll();
        pendingFirstStartCompletion = false;
        flow.enterFatalError();
        renderer.showResourceError();
        return false;
    }
    if (!hasIntro && !hasSpeciesStart && !needsFirstStart)
    {
        if (isFirstLaunchSelectionPending())
            enterFirstLaunch();
        else
            enterCommand();
        return false;
    }

    flow.requestStartup();
    pendingFirstStartCompletion = needsFirstStart;
    Animation sequence[3];
    uint8_t sequenceCount = 0;

    if (hasIntro)
    {
        const unsigned long introDuration = max(
            gameTick,
            static_cast<unsigned long>(animations->frameCountFor(FirmwarePlaybackRole::StartIntro)) *
                animations->frameIntervalFor(FirmwarePlaybackRole::StartIntro));
        sequence[sequenceCount++] = Animation(FirmwarePlaybackRole::StartIntro, introDuration, true);
    }

    if (needsFirstStart)
    {
        const unsigned long firstStartDuration = max(
            gameTick,
            static_cast<unsigned long>(animations->frameCountFor(FirmwarePlaybackRole::FirstStart)) *
                animations->frameIntervalFor(FirmwarePlaybackRole::FirstStart));
        sequence[sequenceCount++] = Animation(FirmwarePlaybackRole::FirstStart, firstStartDuration, true);
    }

    if (hasSpeciesStart)
    {
        const unsigned long startupDuration = max(
            gameTick,
            static_cast<unsigned long>(animations->frameCountFor(FirmwarePlaybackRole::Start)) *
                animations->frameIntervalFor(FirmwarePlaybackRole::Start));
        sequence[sequenceCount++] = Animation(FirmwarePlaybackRole::Start, startupDuration, true);
    }

    if (animations->replace(AnimationSequence(sequence, sequenceCount)) !=
        PlaybackResult::Accepted)
    {
        pendingFirstStartCompletion = false;
        animations->cancelAll();
        flow.enterFatalError();
        renderer.showResourceError();
        return false;
    }
    return true;
#else
    if (isFirstLaunchSelectionPending())
        enterFirstLaunch();
    else
        enterCommand();
    return false;
#endif
}

void Game::completeFirstStartIfReady(const PlaybackTickResult &playbackResult)
{
#if ENABLE_FIRST_START_ANIMATION
    if (!pendingFirstStartCompletion)
        return;

    if (playbackResult.result == PlaybackResult::PlaybackFailed &&
        playbackResult.playbackRole == FirmwarePlaybackRole::FirstStart)
    {
        pendingFirstStartCompletion = false;
        animations->cancelAll();
        flow.enterFatalError();
        return;
    }

    if (animations->hasAnimationPending(FirmwarePlaybackRole::FirstStart))
        return;

    pendingFirstStartCompletion = false;

    petActions->markFirstStartCompleted();
    if (!petActions->saveNow())
        petActions->resetFirstStartCompleted();
#endif
#if !ENABLE_FIRST_START_ANIMATION
    (void)playbackResult;
#endif
}

void Game::handlePlaybackResult(PlaybackResult playbackResult)
{
    if (playbackResult != PlaybackResult::PlaybackFailed)
        return;

    if (renderer.firstAssetDataError() != AssetData::BundleError::None)
    {
        animations->cancelAll();
        flow.enterFatalError();
        renderer.showResourceError();
        return;
    }

    if (pendingEvolution)
    {
        animations->cancelAll();
        completePendingEvolutionIfReady();
        return;
    }

#if ENABLE_GUESS_GAME
    if (flow.isMinigame())
    {
        minigame->onPlaybackFailed();
        return;
    }
#endif

    if (flow.isCommand())
    {
        animations->cancelAll();
        renderer.showResourceError();
    }
}

void Game::enterFirstLaunch()
{
    flow.beginFirstLaunch();
#if ENABLE_GUESS_GAME
    minigame->reset();
#endif
    animations->cancelAll();
    dirtySelect = true;
    startFirstLaunchRequiredCommand();
    animations->requestFullRedraw();
}

void Game::enterCommand()
{
    flow.enterCommand();
    dirtySelect = true;
}
