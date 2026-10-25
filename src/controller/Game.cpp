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
#if ENABLE_COLLECTION_CATALOG
#include "collection/CollectionCatalog.h"
#endif

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
#if ENABLE_COLLECTION_CATALOG
      , collectionCatalog(std::make_unique<CollectionCatalog>(pet, renderer, petBehaviorConfig))
#endif
#if ENABLE_APPEARANCE_SELECTION
      ,
      appearanceSelection(std::make_unique<AppearanceSelectionController>(renderer, appearanceLoader, *layout))
#endif
#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE
      ,
      minigame(std::make_unique<MinigameController>(*animations
#if ENABLE_GUESS_GAME
                  , *petBehaviorRuntime, static_cast<MinigameOutcomeHost &>(*this)
#endif
                  ))
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
    if (enterDeathIfNeeded())
    {
        retryDeathSave();
        handlePlaybackResult(tickPlayback(millis()).result);
        return;
    }
#if ENABLE_COLLECTION_CATALOG
    if (collectionCatalog->active()) return;
#endif

    if (cheatEvolutionPending && !animations->isBusy() &&
        (flow.isCommand() || flow.isMinigame()))
    {
        cheatEvolutionPending = false;
        handleEvolution();
        if (flow.isFatalError())
            return;
    }

    const unsigned long now = millis();
    if (flow.isStartup() && !animations->isBusy())
        enterCommand();

#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE
    if (flow.isMinigame())
    {
        minigame->update();
#if ENABLE_INSTRUCTION_RESPONSE
        uint32_t instructionMask = 0;
        uint8_t instructionCount = 0;
        if (minigame->consumeInstructionCompletion(instructionMask, instructionCount))
        {
            // Consume before commit/save: a failed disk write must never replay arithmetic.
            if (!petBehaviorRuntime->applyInstructionResults(instructionMask, instructionCount))
                renderer.showResourceError("pet state");
            else
            {
                if (enterDeathIfNeeded()) return;
                if (!refreshOutfitUnlockMask(false, false)) renderer.showResourceError();
#if ENABLE_COLLECTION_CATALOG
                collectionSavePending = true;
                updateCollectionProgress(true);
#else
                if (petSaves.saveCollectionProgress(true, true) == PetSaveResult::Failed)
                    renderer.showResourceError("pet state");
#endif
            }
        }
        if (minigame->capturesInstructionInput()) return;
#endif
        if (enterDeathIfNeeded()) return;
        if (!minigame->isActive())
        {
            if (!refreshOutfitUnlockMask(false))
                renderer.showResourceError();
            flow.onMinigameEnded();
            layout->begin();
            animations->requestFullRedraw();
            dirtySelect = true;
            synchronizePlaybackClock();
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
#if ENABLE_COLLECTION_CATALOG
    if (collectionCatalog->active())
    {
        if (!collectionCatalog->redraw()) enterRuntimeFatal();
        return;
    }
#endif
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
#if ENABLE_COLLECTION_CATALOG
    if (collectionCatalog->active())
    {
        if (!collectionCatalog->redraw()) enterRuntimeFatal();
        return;
    }
#endif

    const unsigned long now = millis();
#if ENABLE_APPEARANCE_SELECTION
    if (appearanceSelection->isActive())
    {
        // A STOP wake redraw of the chooser must not resume base music behind it.
        layout->updateValues(pet.statSnapshot());
        appearanceSelection->requestFullRedraw();
        appearanceSelection->render(now);
        dirtySelect = false;
        return;
    }
#endif

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
    pet.configureDeath(petBehaviorConfig.deathEnabled, petBehaviorConfig.deathStatMask);
    if (!renderer.configureAssetBundle(petBehaviorConfig.assetManifest.bundleId))
    {
        return false;
    }

    appearanceLoader.configureRuntimeContract(petBehaviorConfig);
    animations->configureRuntimeContract(petBehaviorConfig);
    commandExecutor->configureRuntimeContract(petBehaviorConfig);
    petSaves.configureRuntimeContract(petBehaviorConfig);
    commands->configure(petBehaviorConfig);
    layout->configureRuntimeContract(petBehaviorConfig);
    runtimeLoadState = RuntimeLoadState::Ready;
    return true;
}

bool Game::refreshOutfitUnlockMask(bool initialize, bool persist)
{
#if !ENABLE_COLLECTION_CATALOG
    persist = true;
#endif
    return appearanceChanges.refreshUnlockState(initialize, persist);
}

AppearanceChangeResult Game::enterSpecies(uint8_t speciesSlot, uint8_t entryOutfitSlot)
{
    const AppearanceChangeResult result = appearanceChanges.changeSpecies(speciesSlot, entryOutfitSlot);
    enterDeathIfNeeded();
    return result;
}

bool Game::saveNow()
{
    return initialized && petSaves.requestSave() != PetSaveResult::Failed;
}

bool Game::startStartupAnimation()
{
    if (pet.isDead()) return false;
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

bool Game::preventsIdleSleep() const
{
#if ENABLE_INSTRUCTION_RESPONSE
    if (capturesInstructionInput()) return true;
#endif
    return animations->hasFinitePlayback();
}

void Game::stopPlaybackForSleep()
{
    animations->cancelAll();
    synchronizePlaybackClock();
}
bool Game::audioActive() const
{
#if ENABLE_SEQUENCE_AUDIO
    return animations->audioActive();
#else
    return false;
#endif
}

bool Game::isCollectionCatalogActive() const
{
#if ENABLE_COLLECTION_CATALOG
    return collectionCatalog->active();
#else
    return false;
#endif
}

bool Game::updateCollectionProgress(bool immediate)
{
#if ENABLE_COLLECTION_CATALOG
    return finishCollectionSave(petSaves.saveCollectionProgress(collectionSavePending, immediate));
#else
    return true;
#endif
}

#if ENABLE_COLLECTION_CATALOG
bool Game::finishCollectionSave(PetSaveResult result)
{
    if (result == PetSaveResult::Failed)
    {
        // A failed commit keeps pending progress for the next complete mutation.
        // Contract failure remains terminal, distinct from storage failure.
        collectionSavePending = true;
        if (renderer.firstAssetDataError() != AssetData::BundleError::None)
            enterRuntimeFatal();
        else
            renderer.showResourceError("pet state");
        return false;
    }
    collectionSavePending = false;
    return true;
}
#endif

#if ENABLE_GUESS_GAME
void Game::onGuessOutcomeApplied()
{
    if (enterDeathIfNeeded()) return;
    // Round and final outcomes are separate complete mutations. Commit each
    // before a following outcome or its animation can change the same stat.
    if (!refreshOutfitUnlockMask(false, false))
    {
        renderer.showResourceError();
        return;
    }
#if ENABLE_COLLECTION_CATALOG
    collectionSavePending = true;
    updateCollectionProgress(true);
#else
    if (petSaves.saveCollectionProgress(true, true) == PetSaveResult::Failed)
        renderer.showResourceError("pet state");
#endif
}
#endif

void Game::startBatteryAnimation()
{
    if (!initialized)
        return;
#if ENABLE_INSTRUCTION_RESPONSE
    if (capturesInstructionInput())
    {
        minigame->reset();
        flow.onMinigameEnded();
        layout->begin();
        synchronizePlaybackClock();
    }
#endif
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
    if (initialized && pet.isDead()) { OnDeathKey(); return; }
    // Menu selection does not mutate pet values. Keep it responsive while
    // historical days settle; minigame decisions still wait for catch-up.
    if (!initialized || (hasPendingTime() && !flow.isCommand()))
        return;
#if ENABLE_COLLECTION_CATALOG
    if (collectionCatalog->active())
    {
        if (!collectionCatalog->move(direction == KeyDirection::Right)) enterRuntimeFatal();
        return;
    }
#endif
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

#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE
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
    if (initialized && pet.isDead()) { OnDeathKey(); return; }
    // Command playback itself postpones day boundaries. Keep replacement
    // responsive while it plays; idle RTC catch-up still precedes new actions.
    if (!initialized || (hasPendingTime() &&
        !(flow.isCommand() && animations->isBusy())))
        return;
#if ENABLE_COLLECTION_CATALOG
    if (collectionCatalog->active())
    {
        if (!collectionCatalog->confirm()) { enterRuntimeFatal(); return; }
        if (!collectionCatalog->active())
        {
            uint8_t species = 0, outfit = 0;
            if (collectionCatalog->takeActivation(species, outfit))
            {
                const AppearanceChangeResult result = appearanceChanges.applyCollectionTarget(species, outfit);
                if (enterDeathIfNeeded()) return;
                if (result == AppearanceChangeResult::ConfigurationFailed ||
                    result == AppearanceChangeResult::UnlockFailed ||
                    result == AppearanceChangeResult::PetStateRejected)
                { enterRuntimeFatal(); return; }
                if (result == AppearanceChangeResult::SaveFailed) renderer.showResourceError("pet state");
                cheatEvolutionPending = false;
                refreshBaseAnimation();
                enterCommand();
            }
            petClock.resume(latestRtcSeconds);
            requestFullRedraw();
        }
        return;
    }
#endif
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
            if (enterDeathIfNeeded()) return;
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

#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE
    if (flow.isMinigame())
    {
        minigame->onConfirm();
        return;
    }
#endif

    if (!flow.isCommand())
        return;

    handleCommandResult(commandExecutor->execute(*commands));
    synchronizePlaybackClock();
}

void Game::refreshBaseAnimation()
{
    animations->setBaseAnimation(petBehaviorRuntime->baseAnimation());
}

bool Game::setStageDaysForCheat(uint32_t value)
{
    if (pet.isDead()) return false;
    if (!initialized || runtimeLoadState != RuntimeLoadState::Ready || flow.isFatalError() ||
        evolution->isActive())
        return false;

    const uint32_t previous = pet.stageDays();
    pet.setStageDays(value);
    if (petSaves.requestSave() == PetSaveResult::Failed)
    {
        pet.setStageDays(previous);
        return false;
    }
    if (!refreshOutfitUnlockMask(false))
        return false;
    // A one-shot may be playing during the hold. Check the new day as soon as
    // normal command playback is idle, before the next daily change.
    cheatEvolutionPending = true;
    return true;
}

PlaybackTickResult Game::tickPlayback(unsigned long now)
{
    layout->setCommandsVisible(!pet.isDead());
    if (animations->takeFullRedrawRequest())
        layout->begin();
    animations->preparePlayback(now);
    if (!layout->syncPlayback(pet.statSnapshot()))
        return {PlaybackResult::PlaybackFailed, animations->currentPlaybackRole()};
    return animations->tick(now);
}

bool Game::completePlaybackTick(const PlaybackTickResult &playbackResult)
{
    if (enterDeathIfNeeded()) return !flow.isFatalError();
    // Keep failure handling before completion records and evolution progression.
    handlePlaybackResult(playbackResult.result);
    if (flow.isFatalError())
        return false;
    completeFirstStartIfReady(playbackResult);
    if (flow.isFatalError())
        return false;
    if (playbackResult.result == PlaybackResult::Accepted)
        handleEvolutionResult(evolution->update(PlaybackResult::Accepted));
    synchronizePlaybackClock();
    return !flow.isFatalError();
}

void Game::synchronizePlaybackClock()
{
    if (!flow.isCommand()
#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE
        && !(flow.isMinigame() && minigame->pausesPetTime())
#endif
    ) return;
#if ENABLE_INSTRUCTION_RESPONSE
    if (capturesInstructionInput())
    {
        petClock.pause(latestRtcSeconds);
        return;
    }
#endif
    if (animations->hasFinitePlayback()) petClock.pause(latestRtcSeconds);
    else petClock.resume(latestRtcSeconds);
}

void Game::handleCommandResult(const CommandResult &result)
{
    if (enterDeathIfNeeded()) return;
    if (!result.executed)
        return;

    if (result.resourceError)
        renderer.showResourceError();

    if (result.actionResult != PetBehaviorActionResult::Rejected)
    {
        handleAppliedUserAction();
        return;
    }

    handleSystemCommandRequest(result);
}

void Game::handleAppliedUserAction()
{
    if (!refreshOutfitUnlockMask(false, false))
        renderer.showResourceError();
#if ENABLE_COLLECTION_CATALOG
    collectionSavePending = true;
#endif
    if (!updateCollectionProgress()) return;
    refreshBaseAnimation();
}

void Game::handleSystemCommandRequest(const CommandResult &result)
{
#if ENABLE_COLLECTION_CATALOG
    if (result.requestedCollection)
    {
        petClock.pause(latestRtcSeconds);
        animations->cancelAll();
        if (!collectionCatalog->start()) enterRuntimeFatal();
        return;
    }
#endif

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

#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE
    if (result.requestedMinigame)
    {
#if ENABLE_INSTRUCTION_RESPONSE
        if (result.commandId == AppCommandId::InstructionResponse)
        {
            petClock.pause(latestRtcSeconds);
            minigame->startInstruction(renderer, petBehaviorConfig);
            flow.enterMinigame();
            return;
        }
#endif
#if ENABLE_COMMAND_PREDICT
        if (result.commandId == AppCommandId::Predict)
        {
            if (minigame->startPredict()) flow.enterMinigame();
            return;
        }
#endif
#if ENABLE_GUESS_GAME
        minigame->startGuessItem();
        flow.enterMinigame();
#endif
        return;
    }
#endif
}

void Game::synchronizeTime(uint32_t rtcSeconds)
{
    latestRtcSeconds = rtcSeconds;
    if (!initialized) return;
    if (enterDeathIfNeeded()) return;
    if (renderer.firstAssetDataError() != AssetData::BundleError::None)
    {
        enterRuntimeFatal();
        return;
    }
    if (isCollectionCatalogActive())
    {
        petClock.pause(rtcSeconds);
        return;
    }
    if (!(flow.isCommand() || flow.isMinigame()))
    {
        clockStarted = false;
        return;
    }
    if (!clockStarted)
    {
        petClock.reset(rtcSeconds);
        clockStarted = true;
        catchUpPrimed = false;
    }
    synchronizePlaybackClock();
    petClock.observe(rtcSeconds);
    if (petClock.due() > 1) petSaves.beginBatch();
    if (petClock.pending())
    {
        // One event boundary per loop keeps playback and voltage sampling live.
        const uint32_t consumedDays = settlePetDays(petClock.due());
        if (pet.isDead()) return;
        if (consumedDays != 0) petClock.consume(consumedDays);
        catchUpPrimed = consumedDays != 0;
    }
    // Time spent draining the snapshot must precede new input too.
    if (initialized && !petClock.pending()) petClock.observe(rtcSeconds);
    if (initialized && !petClock.pending())
    {
        catchUpPrimed = false;
        // Periodic write failures retain their existing retry cadence.
        (void)petSaves.endBatch();
    }
}

uint32_t Game::settlePetDays(uint32_t dueDays)
{
    if (enterDeathIfNeeded()) return 0;
    const EvolutionResult progress = evolution->update(PlaybackResult::Accepted);
    handleEvolutionResult(progress);
    if (progress != EvolutionResult::NoChange || animations->isBusy()) return 0;

    uint32_t days = 1;
    // Settle already-matching unlock/acquisition predicates on the first
    // day before skipping a stable interval (which may end in death).
    if (catchUpPrimed && dueDays > 1 &&
        !petBehaviorRuntime->catchUpDays(appearanceLoader, dueDays, days))
    {
        enterRuntimeFatal();
        return 0;
    }
    if (!petBehaviorRuntime->advancePetDay(days))
    {
        renderer.showResourceError();
        return days;
    }
    if (enterDeathIfNeeded()) return days;
#if ENABLE_DAILY_POST_PROCESSING
#if !ENABLE_COLLECTION_CATALOG
    (void)petSaves.maybeSave();
#endif
    if (!refreshOutfitUnlockMask(false, false))
    {
        renderer.showResourceError();
        return days;
    }
#if ENABLE_COLLECTION_CATALOG
    if (!finishCollectionSave(petSaves.saveDailyProgress(collectionSavePending)))
        return days;
#endif
    if (handleEvolution() != EvolutionResult::NoChange)
        return flow.isFatalError() ? 0 : days;
#endif
    refreshBaseAnimation();
    return days;
}

EvolutionResult Game::handleEvolution()
{
    if (enterDeathIfNeeded()) return EvolutionResult::NoChange;
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
#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE
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
    const auto segmentDuration = [this](FirmwarePlaybackRole role)
    {
        return max(startupSegmentMs,
                   static_cast<unsigned long>(animations->frameCountFor(role)) *
                       animations->frameIntervalFor(role));
    };

    if (hasIntro)
    {
        sequence[sequenceCount++] = Animation(
            FirmwarePlaybackRole::StartIntro, segmentDuration(FirmwarePlaybackRole::StartIntro), true);
    }

    if (needsFirstStart)
    {
        sequence[sequenceCount++] = Animation(
            FirmwarePlaybackRole::FirstStart, segmentDuration(FirmwarePlaybackRole::FirstStart), true);
    }

    if (hasSpeciesStart)
    {
        sequence[sequenceCount++] = Animation(
            FirmwarePlaybackRole::Start, segmentDuration(FirmwarePlaybackRole::Start), true);
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
    if (petSaves.requestSave() == PetSaveResult::Failed)
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
    if (pet.isDead()) { renderer.showResourceError(); return; }

    const EvolutionResult evolutionResult = evolution->update(playbackResult);
    handleEvolutionResult(evolutionResult);
    if (evolutionResult != EvolutionResult::NoChange)
        return;

#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE
    if (flow.isMinigame())
    {
        if (minigame->showsResourceErrorOnPlaybackFailure()) renderer.showResourceError();
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

bool Game::isDead() const { return pet.isDead(); }

void Game::OnDeathKey()
{
    if (initialized && pet.isDead()) resetPet();
}

#if ENABLE_DEATH
bool Game::enterDeathIfNeeded()
{
    if (!pet.evaluateDeath()) return false;
    if (deathPresented) return true;
    deathPresented = true;
    pendingFirstStartCompletion = cheatEvolutionPending = false;
    evolution->cancel();
    animations->cancelAll();
#if ENABLE_COLLECTION_CATALOG
    collectionCatalog->exit();
    collectionSavePending = false;
#endif
#if ENABLE_APPEARANCE_SELECTION
    appearanceSelection->exit();
#endif
#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE
    minigame->reset();
#endif
    flow.enterCommand();
    commands->resetSelection();
    petClock.reset(latestRtcSeconds);
    clockStarted = false;
    petSaves.resetTransientState();
    deathSavePending = petSaves.saveLifeState() != PetSaveResult::Persisted;
    deathSaveRetryAt = millis();
    refreshBaseAnimation();
    animations->requestFullRedraw();
    dirtySelect = false;
    if (deathSavePending) renderer.showResourceError("pet state");
    return true;
}

void Game::retryDeathSave()
{
    if (!deathSavePending || millis() - deathSaveRetryAt < 1000UL) return;
    deathSaveRetryAt = millis();
    deathSavePending = petSaves.saveLifeState() != PetSaveResult::Persisted;
    if (deathSavePending) renderer.showResourceError("pet state");
    else animations->requestFullRedraw();
}
#endif

#if ENABLE_INSTRUCTION_RESPONSE
bool Game::capturesInstructionInput() const { return flow.isMinigame() && minigame->capturesInstructionInput(); }
void Game::instructionInput(uint8_t held, uint8_t pressed, uint32_t timestamp)
{
    if (capturesInstructionInput()) minigame->instructionInput(held, pressed, timestamp);
}
void Game::instructionPending(bool pending, uint32_t at, uint8_t raw)
{
    if (capturesInstructionInput()) minigame->instructionPending(pending, at, raw);
}
#endif
