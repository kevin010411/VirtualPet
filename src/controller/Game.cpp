#include "resources/AssetResources.h"
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

Game::Game(Pet &petRef, PetStorage &petStorageRef, Renderer &rendererRef, AssetResources &resourcesRef, AppearanceLoader &appearanceLoaderRef)
    : pet(petRef),
      renderer(rendererRef),
      resources(resourcesRef),
      appearanceLoader(appearanceLoaderRef),
      petSaves(pet, petStorageRef, resources
#if ENABLE_DEBUG
               , renderer.debugDisplay()
#endif
      ),
      appearanceChanges(pet, petSaves, petStorageRef, resources, appearanceLoader,
                        static_cast<AppearanceChangeHost &>(*this)),
      animations(std::make_unique<AnimationController>(renderer, resources)),
      evolution(std::make_unique<EvolutionController>(
          pet, *animations, appearanceLoader, resources, static_cast<EvolutionHost &>(*this))),
      petBehaviorRuntime(std::make_unique<PetBehaviorRuntime>(runtime.behavior, pet, *animations, resources
#if ENABLE_COLLECTION_GAME_EVENTS
                                                            , runtime.table
#endif
                                                            )),
      petSession(pet, petStorageRef, resources, appearanceLoader, runtime,
                 *petBehaviorRuntime, appearanceChanges, static_cast<PetSessionHost &>(*this)),
      commandExecutor(std::make_unique<CommandExecutor>(
          pet, *animations, *petBehaviorRuntime)),
      commands(std::make_unique<CommandController>()),
      layout(std::make_unique<LayoutRenderer>(renderer, resources, *commands))
#if ENABLE_PROJECT_MENUS
      , projectMenu(renderer, resources, runtime.table.assetManifest)
#endif
#if ENABLE_SHOP
      , shop(pet, petSaves, appearanceChanges, renderer, resources, runtime.behavior
#if ENABLE_COLLECTION_GAME_EVENTS
             , runtime.table
#endif
             )
#endif
#if ENABLE_COLLECTION_CATALOG
      , collectionCatalog(std::make_unique<CollectionCatalog>(pet, renderer, resources, runtime.table))
#endif
#if ENABLE_APPEARANCE_SELECTION
      ,
      appearanceSelection(std::make_unique<AppearanceSelectionController>(renderer, resources, appearanceLoader, *layout))
#endif
#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE || ENABLE_CAPTURE_GAME
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

void Game::loop_game(bool confirmPending)
{
#if !ENABLE_CAPTURE_GAME && !ENABLE_INSTRUCTION_RESPONSE && !ENABLE_COMMAND_PREDICT && !ENABLE_GUESS_GAME
    (void)confirmPending;
#endif
    if (!initialized)
        return;

    if (resources.firstAssetDataError() != AssetData::BundleError::None)
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
    if (flow.is(AppStage::Command) && continuePendingAction()) return;
    if (updateExclusiveForeground()) return;

    if (cheatEvolutionPending && !animations->isBusy() && flow.is(AppStage::Command))
    {
        cheatEvolutionPending = false;
        handleEvolution();
        if (flow.is(AppStage::FatalError))
            return;
    }

    const unsigned long now = millis();
    if (flow.is(AppStage::Startup) && !animations->isBusy())
        enterCommand();

#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE || ENABLE_CAPTURE_GAME
    if (flow.is(AppStage::Minigame))
    {
        const uint32_t previousDays = pet.stageDays();
        minigame->update(confirmPending);
        cheatEvolutionPending |= pet.stageDays() != previousDays;
#if ENABLE_CAPTURE_GAME
        if (minigame->capturesScreen()) return;
#endif
#if ENABLE_INSTRUCTION_RESPONSE
        uint32_t instructionMask = 0;
        uint8_t instructionCount = 0;
        if (minigame->consumeInstructionCompletion(instructionMask, instructionCount))
        {
            const uint32_t previousDays = pet.stageDays();
            // Consume before commit/save: a failed disk write must never replay arithmetic.
            if (!petBehaviorRuntime->applyInstructionResults(runtime.instruction, instructionMask, instructionCount))
                renderer.showResourceError("pet state");
            else
            {
                cheatEvolutionPending |= pet.stageDays() != previousDays;
                settleMutation(Settlement::InstructionCompletion);
                if (pet.isDead()) return;
#if ENABLE_STAT_EVENTS
                if (presentStatEvents(millis())) return;
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
            enterCommand();
            layout->begin();
            animations->requestFullRedraw();
            dirtySelect = true;
            synchronizePlaybackClock();
        }
    }
#endif

#if ENABLE_APPEARANCE_SELECTION
    if (flow.is(AppStage::Appearance))
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

bool Game::continuePendingAction()
{
    if (pet.pendingActionOutcome() == UINT8_MAX)
        return false;

    uint8_t species = 0, outfit = 0;
    const auto progress = petBehaviorRuntime->continuePendingAction(species, outfit);
    if (progress == PendingActionProgress::Playing)
    {
        if (tickPlayback(millis()).result == PlaybackResult::PlaybackFailed)
            enterRuntimeFatal();
        return true;
    }
    if (progress == PendingActionProgress::Failed)
    {
        enterRuntimeFatal();
        return true;
    }
    if (progress == PendingActionProgress::Settled || progress == PendingActionProgress::DayAdvanced)
    {
        if (enterDeathIfNeeded(true)) return true;
#if ENABLE_DAILY_POST_PROCESSING
        if (progress == PendingActionProgress::DayAdvanced &&
            settleMutation(Settlement::DailyProgress) != EvolutionResult::NoChange) return true;
#endif
#if ENABLE_STAT_EVENTS
        if (!statEventsActive())
#endif
            refreshBaseAnimation();
        requestFullRedraw();
        synchronizePlaybackClock();
        return true;
    }

    const PersistedPetState previous = pet.persistentState();
    pet.setPendingActionOutcome(UINT8_MAX);
    if (enterSpecies(species, outfit) != AppearanceChangeResult::Applied)
    {
        pet.restoreState(previous);
        enterRuntimeFatal();
        return true;
    }
    cheatEvolutionPending = false;
    commands->resetSelection();
    refreshBaseAnimation();
    requestFullRedraw();
    synchronizePlaybackClock();
    return true;
}

bool Game::configureActiveAppearance(uint8_t speciesSlot, uint8_t outfitSlot)
{
    char errorResource[20] = {};
    if (!loadRuntimeContract(resources.sdCard(), speciesSlot, outfitSlot,
                             runtime, errorResource, sizeof(errorResource)))
    {
        resources.recordAssetDataErrorResource(errorResource);
        return false;
    }
    return activateLoadedAppearance(speciesSlot, outfitSlot);
}

bool Game::activateLoadedAppearance(uint8_t speciesSlot, uint8_t outfitSlot)
{
    pet.configureDeath(runtime.behavior.deathEnabled, runtime.behavior.deathStatMask);
    if (!resources.configureBundle(runtime.table.assetManifest.bundleId))
    {
        return false;
    }

#if ENABLE_SHOP
    if (!shop.configure(runtime.shopEnabled, runtime.table.assetManifest.bundleId)) return false;
#endif
    appearanceLoader.configureRuntimeContract(runtime.table.assetManifest, runtime.behavior);
    animations->configureRuntimeContract(runtime.playback);
    commandExecutor->configureRuntimeContract(runtime.statusSets, runtime.behavior);
    petSaves.configureRuntimeContract(runtime.table);
    petBehaviorRuntime->setPersistence([](void *context, bool settleActionEffects) {
        auto &game = *static_cast<Game *>(context);
        if (settleActionEffects && !game.pet.isDead() && !game.appearanceChanges.resolveOutfitUnlocks(false))
            return false;
        if (game.petSaves.saveCollectionProgress(true, true) != PetSaveResult::Persisted)
            return false;
#if ENABLE_COLLECTION_CATALOG
        game.collectionSavePending = false;
#endif
        return true;
    }, this);
#if ENABLE_STAT_EVENTS
    if (runtime.statEvents.statEventTriggerCount)
    {
        petBehaviorRuntime->setStatEventSettlement([](void *context, const PetStatSnapshot &after, uint8_t actionSlot, bool daily) {
            return static_cast<Game *>(context)->settleStatMutation(after, actionSlot, daily);
        });
    }
    else
        petBehaviorRuntime->setStatEventSettlement(nullptr);
#endif
    commands->configure(runtime.buttons, runtime.layout);
    layout->configureRuntimeContract(runtime.layout, runtime.behavior, runtime.table.assetManifest.bundleId);
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
#if ENABLE_STAT_EVENTS
    if (isRoundComplete()) return false;
#endif
    if (pet.isDead()) return false;
    if (!initialized)
        return false;
    if (flow.is(AppStage::Battery) || flow.is(AppStage::FatalError)) return false;
    cancelForeground();
    return beginStartupAnimation();
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
    return flow.is(AppStage::Collection);
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
        if (resources.firstAssetDataError() != AssetData::BundleError::None)
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
    settleMutation(Settlement::GuessOutcome);
}
#endif

void Game::refreshBaseAnimation()
{
    animations->setBaseAnimation(petBehaviorRuntime->baseAnimation());
}

bool Game::setStageDaysForCheat(uint32_t value)
{
#if ENABLE_STAT_EVENTS
    if (isRoundComplete()) return false;
#endif
    if (pet.isDead()) return false;
    if (!initialized || runtimeLoadState != RuntimeLoadState::Ready || flow.is(AppStage::FatalError) ||
        evolution->isActive())
        return false;

#if ENABLE_CAPTURE_GAME
    OnCaptureCancel();
#endif
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
    if (enterDeathIfNeeded()) return !flow.is(AppStage::FatalError);
    // Keep failure handling before completion records and evolution progression.
    handlePlaybackResult(playbackResult.result);
    if (flow.is(AppStage::FatalError))
        return false;
    completeFirstStartIfReady(playbackResult);
    if (flow.is(AppStage::FatalError))
        return false;
#if ENABLE_STAT_EVENTS
    if (playingCommittedEvolution)
    {
        if (!animations->hasFinitePlayback())
        {
            playingCommittedEvolution = false;
            flow.endEvolution();
        }
    }
    else
#endif
    if (playbackResult.result == PlaybackResult::Accepted)
        handleEvolutionResult(evolution->update(PlaybackResult::Accepted));
    synchronizePlaybackClock();
    return !flow.is(AppStage::FatalError);
}

void Game::handleCommandResult(const CommandResult &result)
{
    if (enterDeathIfNeeded(result.kind == CommandResultKind::UserAction)) return;
    if (result.resourceError)
        renderer.showResourceError();
    if (result.kind == CommandResultKind::None)
        return;

    if (result.kind == CommandResultKind::UserAction)
    {
#if ENABLE_STAT_EVENTS
        if (statEventsActive()) return;
#endif
        refreshBaseAnimation();
        return;
    }

    handleSystemCommandRequest(result);
}

EvolutionResult Game::settleMutation(Settlement source)
{
    if (enterDeathIfNeeded()) return EvolutionResult::NoChange;
#if ENABLE_STAT_EVENTS
    if (statEventsActive()) return EvolutionResult::NoChange;
#endif
    const bool daily = source == Settlement::DailyProgress;
#if !ENABLE_COLLECTION_CATALOG
    if (daily) (void)petSaves.maybeSave();
#endif
    if (!refreshOutfitUnlockMask(false, false))
    {
        renderer.showResourceError();
        // Instruction still saves committed arithmetic on unlock failure.
        if (daily || source == Settlement::GuessOutcome) return EvolutionResult::NoChange;
    }
#if ENABLE_COLLECTION_CATALOG
    if (!daily) collectionSavePending = true;
    const bool saved = daily
        ? finishCollectionSave(petSaves.saveDailyProgress(collectionSavePending))
        : updateCollectionProgress(true);
    if (!saved) return EvolutionResult::NoChange;
#else
    if (!daily &&
        petSaves.saveCollectionProgress(true, true) == PetSaveResult::Failed)
        renderer.showResourceError("pet state");
#endif
    if (daily)
    {
        const EvolutionResult result = handleEvolution();
        if (result != EvolutionResult::NoChange) return result;
    }
    if (daily) refreshBaseAnimation();
    return EvolutionResult::NoChange;
}

void Game::synchronizeTime(uint32_t rtcSeconds)
{
    latestRtcSeconds = rtcSeconds;
    if (!initialized) return;
    if (enterDeathIfNeeded()) return;
    if (pet.pendingActionOutcome() != UINT8_MAX)
    {
        petClock.pause(rtcSeconds);
        return;
    }
#if ENABLE_STAT_EVENTS
    if (isRoundComplete() || statEventPageCount || committedEvolution.speciesSlot) return;
#endif
    if (resources.firstAssetDataError() != AssetData::BundleError::None)
    {
        enterRuntimeFatal();
        return;
    }
    if (foregroundHoldsPetClock())
    {
        petClock.pause(rtcSeconds);
        return;
    }
    if (runtime.behavior.manualDays)
    {
        petClock.reset(rtcSeconds);
        catchUpPrimed = false;
        return;
    }
    if (!flow.allowsPetTime())
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
#if ENABLE_STAT_EVENTS
        if (statEventPageCount || committedEvolution.speciesSlot || isRoundComplete())
            petClock.pause(rtcSeconds);
#endif
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
    if (!runtime.behavior.manualDays && runtime.behavior.dayResetCount == 0 && catchUpPrimed && dueDays > 1 &&
#if ENABLE_STAT_EVENTS
        !statEventsActive() &&
#endif
        !petBehaviorRuntime->catchUpDays(appearanceLoader, dueDays, days))
    {
        enterRuntimeFatal();
        return 0;
    }
    if (!petBehaviorRuntime->advancePetDay(days))
    {
        renderer.showResourceError("pet state");
#if ENABLE_STAT_EVENTS
        if (statEventsActive()) return 0;
#endif
        return days;
    }
    if (enterDeathIfNeeded()) return days;
#if ENABLE_STAT_EVENTS
    if (statEventsActive()) return days;
#endif
#if ENABLE_DAILY_POST_PROCESSING
    if (settleMutation(Settlement::DailyProgress) != EvolutionResult::NoChange)
        return flow.is(AppStage::FatalError) ? 0 : days;
#else
    refreshBaseAnimation();
#endif
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
#if ENABLE_APPEARANCE_SELECTION
    if (result != EvolutionResult::NoChange && flow.is(AppStage::Appearance))
    {
        // Both animated and immediate Evolution invalidate the old-species options.
        appearanceSelection->exit();
        enterCommand();
    }
#endif
    if (result == EvolutionResult::InProgress) flow.beginEvolution();
    else flow.endEvolution();
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
    cancelForeground();
    flow.enter(AppStage::FatalError);
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

    flow.enter(AppStage::Startup);
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

    if (resources.firstAssetDataError() != AssetData::BundleError::None)
    {
        enterRuntimeFatal();
        return;
    }
    if (pet.isDead()) { renderer.showResourceError(); return; }

    const EvolutionResult evolutionResult = evolution->update(playbackResult);
    handleEvolutionResult(evolutionResult);
    if (evolutionResult != EvolutionResult::NoChange)
        return;

#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE || ENABLE_CAPTURE_GAME
    if (flow.is(AppStage::Minigame))
    {
        if (minigame->showsResourceErrorOnPlaybackFailure()) renderer.showResourceError();
        minigame->onPlaybackFailed();
        return;
    }
#endif

    if (flow.is(AppStage::Command))
    {
        animations->cancelAll();
        renderer.showResourceError();
    }
}

bool Game::isDead() const { return pet.isDead(); }

void Game::OnDeathKey()
{
    if (initialized && pet.isDead()) resetPet();
}

#if ENABLE_DEATH
bool Game::enterDeathIfNeeded(bool persisted)
{
    if (!pet.evaluateDeath()) return false;
    if (deathPresented) return true;
    deathPresented = true;
    pendingFirstStartCompletion = cheatEvolutionPending = false;
    cancelForeground();
#if ENABLE_COLLECTION_CATALOG
    collectionSavePending = false;
#endif
    commands->resetSelection();
    petClock.reset(latestRtcSeconds);
    clockStarted = false;
    petSaves.resetTransientState();
    deathSavePending = !persisted && petSaves.saveLifeState() != PetSaveResult::Persisted;
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
