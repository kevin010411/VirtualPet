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
#include "pet/domain/Pet.h"
#include "presentation/adapters/rendering/Renderer.h"
#include "pet/adapters/PetStorage.h"
#include "shared/config/AppProfile.h"

namespace
{
constexpr uint8_t kEvolutionPlaybackCount = 2;
}

Game::Game(Pet &petRef, PetStorage &petStorageRef, Renderer &rendererRef, AppearanceLoader &appearanceLoaderRef)
    : pet(petRef),
      petStorage(petStorageRef),
      renderer(rendererRef),
      appearanceLoader(appearanceLoaderRef),
      petActions(std::make_unique<PetActionController>(pet, petStorage, renderer, appearanceLoader)),
      animations(std::make_unique<AnimationController>(renderer)),
      petBehaviorRuntime(std::make_unique<PetBehaviorRuntime>(petBehaviorConfig, pet, *animations, renderer)),
      commandExecutor(std::make_unique<CommandExecutor>(
          pet, *animations, *petBehaviorRuntime)),
      commands(std::make_unique<CommandController>(*commandExecutor)),
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
        flow.enterFatalError();

    if (flow.isFatalError())
    {
        renderer.showResourceError();
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
    handlePlaybackResult(playbackResult.result);
    completeFirstStartIfReady(playbackResult);
    if (playbackResult.result == PlaybackResult::Accepted)
        completePendingEvolutionIfReady();

    if (flow.isFatalError())
    {
        renderer.showResourceError();
        return;
    }

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
    handlePlaybackResult(playbackResult.result);
    completeFirstStartIfReady(playbackResult);
    if (playbackResult.result == PlaybackResult::Accepted)
        completePendingEvolutionIfReady();
    if (flow.isFatalError())
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

void Game::setRendererAssetAppearance(uint8_t speciesSlot, uint8_t outfitSlot)
{
    if (runtimeLoadState == RuntimeLoadState::Failed)
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
        runtimeLoadState = RuntimeLoadState::Failed;
        flow.enterFatalError();
        return false;
    }
    return activateLoadedAppearance(speciesSlot, outfitSlot);
}

bool Game::activateLoadedAppearance(uint8_t speciesSlot, uint8_t outfitSlot)
{
    if (!renderer.configureAssetBundle(petBehaviorConfig.assetManifest.bundleId))
    {
        runtimeLoadState = RuntimeLoadState::Failed;
        flow.enterFatalError();
        return false;
    }

    renderer.setAssetAppearance(speciesSlot, outfitSlot);
    renderer.setAnimationArea(0, 0, 0, 0);
    for (uint8_t index = 0; index < petBehaviorConfig.screenBlockCount; ++index)
    {
        const ScreenBlockConfig &block = petBehaviorConfig.screenBlocks[index];
        if (block.kind == ScreenBlockKind::Animation)
            renderer.setAnimationArea(block.x, block.y, block.width, block.height);
    }
    appearanceLoader.configureRuntimeContract(petBehaviorConfig);
    animations->configureRuntimeContract(petBehaviorConfig);
    commandExecutor->configureRuntimeContract(petBehaviorConfig);
    commands->configure(petBehaviorConfig);
    layout->configureRuntimeContract(petBehaviorConfig);
    runtimeLoadState = RuntimeLoadState::Ready;
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
    if (!configureActiveAppearance(speciesSlot, entryOutfitSlot))
    {
#if ENABLE_DEBUG
        startupDebugStage = "enter configure";
#endif
        return false;
    }
    return commitSpeciesAppearance(speciesSlot, entryOutfitSlot);
}

bool Game::commitSpeciesAppearance(uint8_t speciesSlot, uint8_t entryOutfitSlot)
{
    if (!petActions->stageAppearance(speciesSlot, entryOutfitSlot))
    {
        renderer.recordAssetDataErrorResource("pet appearance");
#if ENABLE_DEBUG
        startupDebugStage = "stage appearance";
#endif
        return false;
    }
    if (!resolveOutfitUnlockMask(true))
    {
        renderer.recordAssetDataErrorResource(appearanceLoader.firstAssetDataErrorResource());
#if ENABLE_DEBUG
        startupDebugStage = "outfit unlock";
#endif
        return false;
    }
    if (!petActions->saveNow())
    {
        renderer.recordAssetDataErrorResource(
            petStorage.lastSaveSlot() == 'B' ? "state_b.bin" : "state_a.bin");
#if ENABLE_DEBUG
        startupDebugStage = "pet state save";
#endif
        return false;
    }
    return true;
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
    clearPendingEvolution();
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
    if (!initialized)
        return;
    if (pendingEvolutionPhase != PendingEvolutionPhase::None)
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
    if (pendingEvolutionPhase != PendingEvolutionPhase::None)
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
    if (pendingEvolutionPhase != PendingEvolutionPhase::None)
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
            if (configureActiveAppearance(pet.speciesSlot(), selectedOutfit))
            {
                bool applied = false;
                if (requiresUnlock)
                {
                    PetStatSnapshot consumedStats = {};
                    if (appearanceLoader.resolveConsumableOutfitUnlock(
                            pet.speciesSlot(), selectedOutfit,
                            pet.statSnapshot(), consumedStats))
                        applied = petActions->applyConsumableOutfitUnlock(
                            selectedOutfit, consumedStats);
                }
                else
                    applied = petActions->applyAppearance(
                        pet.speciesSlot(), selectedOutfit);
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
    if (!commands->isSlotVisible(selectedSlot))
        return;
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
        }
        return;
    }
    commandExecutor->begin(commandId);
    const bool executed = commands->executeCurrent();
    handleCommandResult(commandExecutor->complete(executed), selectedSlot);
}

void Game::refreshBaseAnimation()
{
    animations->setBaseAnimation(petBehaviorRuntime->baseAnimation());
}

bool Game::setStageDaysForCheat(uint32_t value)
{
    if (!initialized || runtimeLoadState != RuntimeLoadState::Ready || flow.isFatalError() ||
        pendingEvolutionPhase != PendingEvolutionPhase::None)
        return false;

    const uint32_t previous = pet.stageDays();
    pet.setStageDays(value);
    if (!petActions->saveNow())
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
    if (!syncSceneLayoutWithPlayback())
        return {PlaybackResult::PlaybackFailed, animations->currentPlaybackRole()};
    return animations->tick(now);
}

bool Game::syncSceneLayoutWithPlayback()
{
    uint8_t layoutId = 0;
    if (!renderer.currentLayoutId(layoutId))
    {
        renderer.recordAssetDataErrorResource("asset data");
        return false;
    }
    layout->updateValues(pet.statSnapshot());
    return layout->updatePlayback(layoutId);
}

void Game::handleCommandResult(const CommandResult &result, int selectedSlot)
{
    if (!result.executed)
        return;

    if (result.resourceError)
        renderer.showResourceError();

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
        pet.markFirstLaunchComplete();
        petActions->saveNow();
    }

    if (shouldStart)
        beginStartupAnimation();
    else if (!flow.isFirstLaunch())
        enterCommand();
}

bool Game::isFirstLaunchSelectionPending() const
{
    return false;
}

bool Game::startFirstLaunchRequiredCommand()
{
#if ENABLE_APPEARANCE_SELECTION
    switch (flow.firstLaunchRequiredCommand())
    {
    case AppCommandId::ChangeOutfit:
        if (appearanceSelection->start(pet.speciesSlot(), pet.outfitSlot(),
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

    if (pendingEvolutionPhase != PendingEvolutionPhase::None)
        return;

    if (!animations->isBusy())
    {
        handleEvolution();
        if (flow.isFatalError())
            return;
        if (pendingEvolutionPhase != PendingEvolutionPhase::None)
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
        if (flow.isFatalError())
            return;
        if (pendingEvolutionPhase != PendingEvolutionPhase::None)
            return;

    }

    refreshBaseAnimation();
    petActions->maybeSave();
}

bool Game::completePendingEvolutionIfReady()
{
    if (pendingEvolutionPhase == PendingEvolutionPhase::None)
        return false;

    if (animations->isBusy())
        return false;

    if (pendingEvolutionPhase == PendingEvolutionPhase::SourceSegment)
        pendingEvolutionPhase = PendingEvolutionPhase::ApplyingTarget;

    if (pendingEvolutionPhase == PendingEvolutionPhase::ApplyingTarget &&
        !enterSpecies(pendingEvolutionSpeciesSlot, pendingEvolutionOutfitSlot))
    {
        renderer.showResourceError();
        clearPendingEvolution();
        return false;
    }

    if (pendingEvolutionPhase == PendingEvolutionPhase::ApplyingTarget &&
        pendingEvolutionTargetAnimation.valid())
    {
        const Animation targetAnimation = Animation::complete(
            pendingEvolutionTargetAnimation, kEvolutionPlaybackCount, FirmwarePlaybackRole::Evolution);
        if (animations->replace(AnimationSequence(&targetAnimation, 1)) == PlaybackResult::Accepted)
        {
            pendingEvolutionPhase = PendingEvolutionPhase::TargetSegment;
            return true;
        }
        renderer.showResourceError();
    }

    pendingEvolutionPhase = PendingEvolutionPhase::Complete;
    // Species entry changes the in-memory appearance before its single
    // persistence write. Always hand rendering back to the current base
    // animation so the display cannot remain on the completed Evolution frame.
    refreshBaseAnimation();
    animations->requestFullRedraw();
    clearPendingEvolution();
    return true;
}

void Game::clearPendingEvolution()
{
    pendingEvolutionPhase = PendingEvolutionPhase::None;
    pendingEvolutionSpeciesSlot = 0;
    pendingEvolutionOutfitSlot = 0;
    pendingEvolutionTargetAnimation = {};
}

void Game::handleEvolution()
{
    AppearanceSelection selection = {};
    const EvolutionLookupResult result = petActions->findEvolutionTarget(selection);
    if (result != EvolutionLookupResult::Found)
    {
        if (result == EvolutionLookupResult::LoadFailed)
        {
            runtimeLoadState = RuntimeLoadState::Failed;
            flow.enterFatalError();
            const char *resource = appearanceLoader.firstAssetDataErrorResource();
            renderer.recordAssetDataErrorResource(resource);
            renderer.showResourceError();
        }
        return;
    }

    if (selection.evolutionMode == EvolutionAnimationMode::Disabled)
    {
        if (enterSpecies(selection.speciesSlot, selection.outfitSlot))
        {
            refreshBaseAnimation();
            animations->requestFullRedraw();
        }
        else
            renderer.showResourceError();
        return;
    }

    if (!beginEvolutionAnimation(selection))
        renderer.showResourceError();
}

bool Game::beginEvolutionAnimation(const AppearanceSelection &selection)
{
    if (!animations->hasAnimation(selection.sourceEvolutionAnimation))
        return false;

    pendingEvolutionSpeciesSlot = selection.speciesSlot;
    pendingEvolutionOutfitSlot = selection.outfitSlot;
    pendingEvolutionTargetAnimation = selection.targetEvolutionAnimation;
    pendingEvolutionPhase = PendingEvolutionPhase::SourceSegment;

    const Animation animation = Animation::complete(
        selection.sourceEvolutionAnimation,
        kEvolutionPlaybackCount,
        FirmwarePlaybackRole::Evolution);
    const PlaybackResult replaceResult = animations->replace(AnimationSequence(&animation, 1));
    if (replaceResult != PlaybackResult::Accepted)
    {
        clearPendingEvolution();
        return false;
    }
    return true;
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

    pet.markFirstStartCompleted();
    if (!petActions->saveNow())
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
        animations->cancelAll();
        clearPendingEvolution();
        flow.enterFatalError();
        renderer.showResourceError();
        return;
    }

    if (pendingEvolutionPhase != PendingEvolutionPhase::None)
    {
        animations->cancelAll();
        if (pendingEvolutionPhase == PendingEvolutionPhase::SourceSegment ||
            pendingEvolutionPhase == PendingEvolutionPhase::ApplyingTarget)
        {
            clearPendingEvolution();
            renderer.showResourceError();
        }
        else
        {
            pendingEvolutionPhase = PendingEvolutionPhase::Complete;
            completePendingEvolutionIfReady();
        }
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
