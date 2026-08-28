#include "presentation/application/Game.h"

#include <string.h>
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
    if (petBehaviorLoadingFailed || !loadPetBehaviorContract(animations->sdCard(), petBehaviorConfig))
    {
        petBehaviorLoadingFailed = true;
        petBehaviorLoaded = false;
        startupConfigError = "runtime_contract.txt";
        return false;
    }
    petBehaviorLoaded = true;
    appearanceLoader.configureRuntimeContract(petBehaviorConfig);
    commandExecutor->configureRuntimeContract(petBehaviorConfig);
    commands->configure(petBehaviorConfig);
    commands->resetSelection();
    layout->begin();

    dirtySelect = true;
    pendingEvolution = false;
    pendingFirstStartCompletion = false;
    pendingEvolutionSpeciesCode[0] = '\0';
    pendingEvolutionOutfitCode[0] = '\0';
    last_tick_time = millis();
#if ENABLE_APPEARANCE_SELECTION
    appearanceSelection->exit();
#endif
#if ENABLE_GUESS_GAME
    minigame->reset();
#endif

    if (!loadInitialPetState(true, false))
    {
        initialStateLoadingFailed = true;
        return false;
    }

    // A successfully restored appearance is authoritative. Re-evaluating the
    // evolution table here can immediately replace a saved later-stage species
    // with an earlier wildcard/fallback match before the startup animation.
    // Evolution is still evaluated by handleEvolution() during normal ticks.
    renderer.setAssetAppearance(petActions->speciesCode(), petActions->outfitCode());
    // Animation setup reloads the manifest and selects the initial Idle
    // variant, so it must run after the active appearance is known.
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

    initialized = true;
    enterCommand();
    return true;
}

void Game::loop_game()
{
    if (!initialized)
        return;

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
            flow.onMinigameEnded();
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

void Game::setRendererAssetAppearance(const char *speciesCode, const char *outfitCode)
{
    if (petBehaviorLoadingFailed)
    {
        renderer.showResourceError();
        return;
    }
    if (!pet.setSpeciesCode(speciesCode) || !pet.setOutfitCode(outfitCode))
    {
        initialized = false;
        renderer.showResourceError();
        return;
    }

    renderer.setAssetAppearance(pet.speciesCode(), pet.outfitCode());
    renderer.reloadManifest();
    refreshBaseAnimation();
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
            char selectedSpecies[9] = {};
            char selectedOutfit[9] = {};
            const bool confirmed = appearanceSelection->onConfirmSpecies(
                selectedSpecies,
                sizeof(selectedSpecies),
                selectedOutfit,
                sizeof(selectedOutfit));
            if (confirmed)
            {
                petActions->applyAppearance(selectedSpecies, selectedOutfit);
                refreshBaseAnimation();
            }
            animations->requestFullRedraw();
            completeFirstLaunchIfNeeded(AppCommandId::ChangeSpecies);
            return;
        }

        char selectedOutfit[9] = {};
        const bool confirmed = appearanceSelection->onConfirm(selectedOutfit, sizeof(selectedOutfit));
        if (confirmed)
        {
            petActions->applyAppearance(petActions->speciesCode(), selectedOutfit);
            refreshBaseAnimation();
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
            refreshBaseAnimation();
            layout->enterAction(animations->currentAnimationId(), selectedSlot);
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
    if (!loadInitialPetState(false))
        return false;

    pendingEvolution = false;
    pendingFirstStartCompletion = false;
    pendingEvolutionSpeciesCode[0] = '\0';
    pendingEvolutionOutfitCode[0] = '\0';
    petActions->resetFirstStartCompleted();
    petActions->applyEvolutionTarget();
    renderer.setAssetAppearance(petActions->speciesCode(), petActions->outfitCode());
    renderer.reloadManifest();
    animations->cancelAll();
#if ENABLE_GUESS_GAME
    minigame->reset();
#endif
    refreshBaseAnimation();
    animations->requestFullRedraw();
    petActions->saveNow();
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
        layout->updateAction(animations->currentAnimationId());
}

void Game::handleCommandResult(const CommandResult &result, int selectedSlot)
{
    if (!result.executed)
        return;

    layout->enterAction(result.layoutId, selectedSlot);
    if (result.resourceError)
        renderer.showResourceError();

#if ENABLE_COMMAND_OUTFIT
    if (result.requestedOutfit)
    {
        if (appearanceSelection->start(petActions->speciesCode(), petActions->outfitCode()))
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
        if (appearanceSelection->startSpecies(petActions->speciesCode()))
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

bool Game::loadInitialPetState(bool allowSavedState, bool showError)
{
    if (allowSavedState && petStorage.load(pet, petBehaviorConfig.schemaFingerprint))
    {
        return true;
    }

    AppearanceSelection initialAppearance = {};
    if (!appearanceLoader.findInitialAppearance(initialAppearance))
    {
        if (showError)
            renderer.showResourceError();
        return false;
    }

    pet.setDefaultState();
    pet.setSchemaFingerprint(petBehaviorConfig.schemaFingerprint);
    petBehaviorRuntime->initializeStats();
    const bool applied = pet.setSpeciesCode(initialAppearance.speciesCode) &&
                         pet.setOutfitCode(initialAppearance.outfitCode);
    if (!applied && showError)
        renderer.showResourceError();
    return applied;
}

bool Game::startFirstLaunchRequiredCommand()
{
#if ENABLE_APPEARANCE_SELECTION
    switch (flow.firstLaunchRequiredCommand())
    {
    case AppCommandId::ChangeOutfit:
        if (appearanceSelection->start(petActions->speciesCode(), petActions->outfitCode()))
        {
            animations->cancelAll();
            animations->requestFullRedraw();
            return true;
        }
        break;
    case AppCommandId::ChangeSpecies:
        if (appearanceSelection->startSpecies(petActions->speciesCode()))
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

    petActions->applyAppearance(pendingEvolutionSpeciesCode, pendingEvolutionOutfitCode);
    pendingEvolution = false;
    pendingEvolutionSpeciesCode[0] = '\0';
    pendingEvolutionOutfitCode[0] = '\0';
    // applyAppearance() can report persistence failure after it has already
    // changed the in-memory appearance and reloaded its manifest.  Always
    // hand rendering back to the current base animation so that the display
    // cannot remain on the completed Evolution frame.
    refreshBaseAnimation();
    animations->requestFullRedraw();
    return true;
}

void Game::handleEvolution()
{
    AppearanceSelection selection = {};
    if (!petActions->findEvolutionTarget(selection))
        return;

    if (!beginEvolutionAnimation(selection))
        petActions->applyAppearance(selection.speciesCode, selection.outfitCode);
}

bool Game::beginEvolutionAnimation(const AppearanceSelection &selection)
{
    if (!animations->hasAnimation(AnimationId::Evolution))
        return false;

    strncpy(pendingEvolutionSpeciesCode, selection.speciesCode, sizeof(pendingEvolutionSpeciesCode) - 1);
    pendingEvolutionSpeciesCode[sizeof(pendingEvolutionSpeciesCode) - 1] = '\0';
    strncpy(pendingEvolutionOutfitCode, selection.outfitCode, sizeof(pendingEvolutionOutfitCode) - 1);
    pendingEvolutionOutfitCode[sizeof(pendingEvolutionOutfitCode) - 1] = '\0';
    pendingEvolution = true;

    const Animation animation = Animation::complete(AnimationId::Evolution, 2);
    const PlaybackResult replaceResult = animations->replace(AnimationSequence(&animation, 1));
    if (replaceResult != PlaybackResult::Accepted)
    {
        pendingEvolution = false;
        pendingEvolutionSpeciesCode[0] = '\0';
        pendingEvolutionOutfitCode[0] = '\0';
        return false;
    }
    return true;
}

bool Game::beginStartupAnimation()
{
#if ENABLE_STARTUP_ANIMATION
    const bool hasIntro = animations->hasAnimation(AnimationId::StartIntro);
    const bool hasSpeciesStart = animations->hasAnimation(AnimationId::Start);
    const bool needsFirstStart = ENABLE_FIRST_START_ANIMATION && !petActions->isFirstStartCompleted();
    const bool hasFirstStart = animations->hasAnimation(AnimationId::FirstStart);
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
            static_cast<unsigned long>(animations->frameCountFor(AnimationId::StartIntro)) *
                animations->frameIntervalFor(AnimationId::StartIntro));
        sequence[sequenceCount++] = Animation(AnimationId::StartIntro, introDuration, true);
    }

    if (needsFirstStart)
    {
        const unsigned long firstStartDuration = max(
            gameTick,
            static_cast<unsigned long>(animations->frameCountFor(AnimationId::FirstStart)) *
                animations->frameIntervalFor(AnimationId::FirstStart));
        sequence[sequenceCount++] = Animation(AnimationId::FirstStart, firstStartDuration, true);
    }

    if (hasSpeciesStart)
    {
        const unsigned long startupDuration = max(
            gameTick,
            static_cast<unsigned long>(animations->frameCountFor(AnimationId::Start)) *
                animations->frameIntervalFor(AnimationId::Start));
        sequence[sequenceCount++] = Animation(AnimationId::Start, startupDuration, true);
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
        playbackResult.animationId == AnimationId::FirstStart)
    {
        pendingFirstStartCompletion = false;
        animations->cancelAll();
        flow.enterFatalError();
        return;
    }

    if (animations->hasAnimationPending(AnimationId::FirstStart))
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
