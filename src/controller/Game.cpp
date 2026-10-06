#include "controller/Game.h"

#include "animation/AnimationController.h"
#include "controller/CommandController.h"
#include "controller/CommandExecutor.h"
#include "display/LayoutRenderer.h"
#include "controller/MinigameController.h"
#if ENABLE_APPEARANCE_SELECTION
#include "appearance/AppearanceSelectionController.h"
#endif
#include "pet/PetSaveController.h"
#include "pet/PetBehaviorRuntime.h"
#include "pet/Pet.h"
#include "display/Renderer.h"
#include "pet/PetStorage.h"
#include "common/AppProfile.h"

Game::Game(Pet &petRef, PetStorage &petStorageRef, Renderer &rendererRef, AppearanceLoader &appearanceLoaderRef)
    : pet(petRef),
      renderer(rendererRef),
      appearanceLoader(appearanceLoaderRef),
      petSaves(pet, petStorageRef, renderer),
      appearanceChanges(pet, petSaves, petStorageRef, renderer, appearanceLoader,
                        static_cast<AppearanceChangeHost &>(*this)),
      animations(std::make_unique<AnimationController>(renderer)),
      evolution(std::make_unique<EvolutionController>(
          pet, *animations, appearanceLoader, renderer, static_cast<EvolutionHost &>(*this))),
      petBehaviorRuntime(std::make_unique<PetBehaviorRuntime>(petBehaviorConfig, pet, *animations, renderer)),
      petSession(pet, petStorageRef, renderer, appearanceLoader, petBehaviorConfig,
                 *petBehaviorRuntime, appearanceChanges, static_cast<PetSessionHost &>(*this)),
      commandExecutor(std::make_unique<CommandExecutor>(
          pet, *animations, *petBehaviorRuntime)),
      commands(std::make_unique<CommandController>()),
      layout(std::make_unique<LayoutRenderer>(renderer, *commands))
#if ENABLE_APPEARANCE_SELECTION
      ,
      appearanceSelection(std::make_unique<AppearanceSelectionController>(renderer, appearanceLoader, *layout))
#endif
#if ENABLE_GUESS_GAME
      ,
      minigame(std::make_unique<MinigameController>(*animations, *petBehaviorRuntime))
#endif
{
}

Game::~Game()
    = default;

void Game::loop_game()
{
    if (!initialized)
        return;

    if (renderer.firstAssetDataError() != AssetData::BundleError::None)
    {
        enterRuntimeFatal();
        return;
    }

    if (cheatEvolutionPending && !animations->isBusy() &&
        (flow.isCommand() || flow.isMinigame()))
    {
        cheatEvolutionPending = false;
        handleEvolution();
        if (flow.isFatalError())
            return;
    }

    const unsigned long now = millis();
    const unsigned long elapsed = now - last_tick_time;

    if (elapsed >= gameTick)
    {
        last_tick_time = now;

        if (flow.isCommand() || flow.isMinigame())
            maybeTickPet();
        if (flow.isFatalError())
            return;

        if (flow.isStartup() && !animations->isBusy())
            enterCommand();
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
        layout->updateValues(pet.statSnapshot());
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

    const PlaybackTickResult playbackResult = tickPlayback(now);
    if (!completePlaybackTick(playbackResult))
        return;

    layout->updateValues(pet.statSnapshot());
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
    if (!initialized)
        return;
    dirtySelect = true;
    animations->requestFullRedraw();
#if ENABLE_APPEARANCE_SELECTION
    if (appearanceSelection->isActive())
        appearanceSelection->requestFullRedraw();
#endif
}

void Game::redrawAllNow()
{
    if (!initialized)
        return;

    const unsigned long now = millis();

    // Repaint the entire animation area even when an animation frame was already
    // considered current before STOP mode.
    animations->requestFullRedraw();
    const PlaybackTickResult playbackResult = tickPlayback(now);
    if (!completePlaybackTick(playbackResult))
        return;

#if ENABLE_APPEARANCE_SELECTION
    if (appearanceSelection->isActive())
    {
        appearanceSelection->requestFullRedraw();
        appearanceSelection->render(now);
    }
#endif

    layout->updateValues(pet.statSnapshot());
    dirtySelect = false;

#if ENABLE_DEBUG
    renderer.renderDebugOverlay();
#endif
}

bool Game::configureActiveAppearance(uint8_t speciesSlot, uint8_t outfitSlot)
{
    char errorResource[20] = {};
    if (!loadRuntimeContract(animations->sdCard(), speciesSlot, outfitSlot,
                             petBehaviorConfig, errorResource, sizeof(errorResource)))
    {
        renderer.recordAssetDataErrorResource(errorResource);
        return false;
    }
    return activateLoadedAppearance(speciesSlot, outfitSlot);
}

bool Game::activateLoadedAppearance(uint8_t speciesSlot, uint8_t outfitSlot)
{
    if (!renderer.configureAssetBundle(petBehaviorConfig.assetManifest.bundleId))
    {
        return false;
    }

    renderer.setAssetAppearance(speciesSlot, outfitSlot);
    appearanceLoader.configureRuntimeContract(petBehaviorConfig);
    animations->configureRuntimeContract(petBehaviorConfig);
    commandExecutor->configureRuntimeContract(petBehaviorConfig);
    commands->configure(petBehaviorConfig);
    layout->configureRuntimeContract(petBehaviorConfig);
    runtimeLoadState = RuntimeLoadState::Ready;
    return true;
}

bool Game::refreshOutfitUnlockMask(bool initialize)
{
    return appearanceChanges.refreshUnlockState(initialize);
}

AppearanceChangeResult Game::enterSpecies(uint8_t speciesSlot, uint8_t entryOutfitSlot)
{
    return appearanceChanges.changeSpecies(speciesSlot, entryOutfitSlot);
}

bool Game::saveNow()
{
    return initialized && petSaves.saveNow();
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
    evolution->cancel();
    flow.enterBattery();
    animations->startBatteryAnimation();
    handlePlaybackResult(tickPlayback(millis()).result);
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
    routeDirectionKey(KeyDirection::Left);
}

void Game::OnRightKey()
{
    routeDirectionKey(KeyDirection::Right);
}

void Game::routeDirectionKey(KeyDirection direction)
{
    if (!initialized)
        return;
    if (evolution->isActive())
        return;
#if ENABLE_APPEARANCE_SELECTION
    if (appearanceSelection->isActive())
    {
        if (direction == KeyDirection::Left)
            appearanceSelection->onLeft();
        else
            appearanceSelection->onRight();
        return;
    }
#endif

#if ENABLE_GUESS_GAME
    if (flow.isMinigame())
    {
        if (direction == KeyDirection::Left)
            minigame->onLeft();
        else
            minigame->onRight();
        return;
    }
#endif

    if (flow.isCommand())
    {
        if (direction == KeyDirection::Left)
            commands->prev();
        else
            commands->next();
        dirtySelect = true;
    }
}

void Game::OnConfirmKey()
{
    if (!initialized)
        return;
    if (evolution->isActive())
        return;
#if ENABLE_APPEARANCE_SELECTION
    if (appearanceSelection->isActive())
    {
        uint8_t selectedOutfit = 0;
        bool requiresUnlock = false;
        const bool confirmed = appearanceSelection->onConfirm(selectedOutfit, requiresUnlock);
        if (confirmed)
        {
            const AppearanceChangeResult result =
                appearanceChanges.applyOutfit(selectedOutfit, requiresUnlock);
            if (result == AppearanceChangeResult::Applied)
            {
                appearanceSelection->exit();
                refreshBaseAnimation();
            }
            else if (result == AppearanceChangeResult::ConfigurationFailed)
            {
                enterRuntimeFatal();
                return;
            }
        }
        animations->requestFullRedraw();
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

    if (!flow.isCommand())
        return;

    handleCommandResult(commandExecutor->execute(*commands));
}

void Game::refreshBaseAnimation()
{
    animations->setBaseAnimation(petBehaviorRuntime->baseAnimation());
}

bool Game::setStageDaysForCheat(uint32_t value)
{
    if (!initialized || runtimeLoadState != RuntimeLoadState::Ready || flow.isFatalError() ||
        evolution->isActive())
        return false;

    const uint32_t previous = pet.stageDays();
    pet.setStageDays(value);
    if (!petSaves.saveNow())
    {
        pet.setStageDays(previous);
        return false;
    }
    if (!refreshOutfitUnlockMask(false))
        return false;
    // A one-shot may be playing during the hold. Check the new day as soon as
    // normal command playback is idle, before the next daily change.
    cheatEvolutionPending = true;
    refreshBaseAnimation();
    animations->requestFullRedraw();
    return true;
}

PlaybackTickResult Game::tickPlayback(unsigned long now)
{
    if (animations->takeFullRedrawRequest())
        layout->begin();
    animations->preparePlayback(now);
    if (!layout->syncPlayback(pet.statSnapshot()))
        return {PlaybackResult::PlaybackFailed, animations->currentPlaybackRole()};
    return animations->tick(now);
}

bool Game::completePlaybackTick(const PlaybackTickResult &playbackResult)
{
    // Keep failure handling before completion records and evolution progression.
    handlePlaybackResult(playbackResult.result);
    if (flow.isFatalError())
        return false;
    completeFirstStartIfReady(playbackResult);
    if (flow.isFatalError())
        return false;
    if (playbackResult.result == PlaybackResult::Accepted)
        handleEvolutionResult(evolution->update(PlaybackResult::Accepted));
    return !flow.isFatalError();
}

void Game::handleCommandResult(const CommandResult &result)
{
    if (!result.executed)
        return;

    if (result.resourceError)
        renderer.showResourceError();

    if (result.actionResult != PetBehaviorActionResult::Rejected)
    {
        if (!refreshOutfitUnlockMask(false))
            renderer.showResourceError();
        refreshBaseAnimation();
        return;
    }

#if ENABLE_COMMAND_OUTFIT
    if (result.requestedOutfit)
    {
        if (appearanceSelection->start(pet.speciesSlot(), pet.outfitSlot(),
                                       pet.outfitUnlockMask()))
        {
            animations->cancelAll();
            animations->requestFullRedraw();
        }
        return;
    }
#endif

#if ENABLE_GUESS_GAME
    if (result.requestedMinigame)
    {
        minigame->startGuessItem();
        flow.enterMinigame();
        return;
    }
#endif

}

void Game::maybeTickPet()
{
    const EvolutionResult progress = evolution->update(PlaybackResult::Accepted);
    handleEvolutionResult(progress);
    if (progress != EvolutionResult::NoChange)
        return;

    if (!animations->isBusy())
    {
        if (handleEvolution() != EvolutionResult::NoChange)
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
        if (handleEvolution() != EvolutionResult::NoChange)
            return;

    }

    refreshBaseAnimation();
    petSaves.maybeSave();
}

EvolutionResult Game::handleEvolution()
{
    const EvolutionResult result = evolution->check();
    handleEvolutionResult(result);
    return result;
}

void Game::handleEvolutionResult(EvolutionResult result)
{
    if (result == EvolutionResult::FatalFailure)
        enterRuntimeFatal();
    else if (result == EvolutionResult::Failed)
        renderer.showResourceError();
}

void Game::enterFatalState()
{
    initialized = false;
    runtimeLoadState = RuntimeLoadState::Failed;
    pendingFirstStartCompletion = false;
    cheatEvolutionPending = false;
    animations->cancelAll();
    evolution->cancel();
#if ENABLE_APPEARANCE_SELECTION
    appearanceSelection->exit();
#endif
#if ENABLE_GUESS_GAME
    minigame->reset();
#endif
    flow.enterFatalError();
}

void Game::enterRuntimeFatal()
{
    enterFatalState();
    renderer.showResourceError();
}

bool Game::beginStartupAnimation()
{
#if ENABLE_STARTUP_ANIMATION
    const bool hasIntro = animations->hasAnimation(FirmwarePlaybackRole::StartIntro);
    const bool hasSpeciesStart = animations->hasAnimation(FirmwarePlaybackRole::Start);
    const bool needsFirstStart = ENABLE_FIRST_START_ANIMATION && !pet.isFirstStartCompleted();
    const bool hasFirstStart = animations->hasAnimation(FirmwarePlaybackRole::FirstStart);
    if (needsFirstStart && !hasFirstStart)
    {
        enterRuntimeFatal();
        return false;
    }
    if (!hasIntro && !hasSpeciesStart && !needsFirstStart)
    {
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
        enterRuntimeFatal();
        return false;
    }
    return true;
#else
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
        enterRuntimeFatal();
        return;
    }

    if (animations->hasAnimationPending(FirmwarePlaybackRole::FirstStart))
        return;

    pendingFirstStartCompletion = false;

    pet.markFirstStartCompleted();
    if (!petSaves.saveNow())
        pet.resetFirstStartCompleted();
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
        enterRuntimeFatal();
        return;
    }

    const EvolutionResult evolutionResult = evolution->update(playbackResult);
    handleEvolutionResult(evolutionResult);
    if (evolutionResult != EvolutionResult::NoChange)
        return;

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

void Game::enterCommand()
{
    flow.enterCommand();
    dirtySelect = true;
}
