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
#include "pet/PetBehaviorRuntime.h"
#include "pet/Pet.h"
#include "display/Renderer.h"
#if ENABLE_COLLECTION_CATALOG
#include "collection/CollectionCatalog.h"
#endif

#if ENABLE_SHOP
void Game::redrawShop()
{
    if (!shop.redraw()) enterRuntimeFatal();
}

void Game::finishShopPlayback(bool restoreDisplay)
{
    if (shopEvolutionPending)
    {
        shopEvolutionPending = false;
        enterCommand();
        const EvolutionResult result = handleEvolution();
        if (result != EvolutionResult::NoChange && result != EvolutionResult::Failed)
        {
            shop.close();
            if (result == EvolutionResult::Completed) requestFullRedraw();
            return;
        }
        flow.enter(AppStage::Shop);
        restoreDisplay |= result == EvolutionResult::Failed;
    }
    if (restoreDisplay) redrawShop();
}
#endif

#if ENABLE_PROJECT_MENUS
void Game::redrawProjectMenu()
{
    if (!projectMenu.redraw()) enterRuntimeFatal();
}
#endif

bool Game::updateExclusiveForeground()
{
#if ENABLE_STAT_EVENTS
    if (presentStatEvents(millis())) return true;
#endif
#if ENABLE_SHOP
    if (flow.is(AppStage::Shop))
    {
        if (shopPlayback)
        {
            const PlaybackTickResult playback = tickPlayback(millis());
            if (playback.result == PlaybackResult::PlaybackFailed)
                enterRuntimeFatal();
            else if (!animations->hasFinitePlayback())
            {
                shopPlayback = false;
                animations->cancelAll();
                finishShopPlayback(true);
            }
        }
        else if (shopEvolutionPending)
            finishShopPlayback();
        return true;
    }
#endif
#if ENABLE_COLLECTION_CATALOG
    if (flow.is(AppStage::Collection))
    {
        if (collectionReplayPlaying)
        {
            if (tickPlayback(millis()).result == PlaybackResult::PlaybackFailed)
            { enterRuntimeFatal(); return true; }
            if (!animations->hasFinitePlayback())
            {
                collectionReplayPlaying = false;
                animations->cancelAll();
                if (!collectionCatalog->redraw()) enterRuntimeFatal();
            }
        }
        return true;
    }
#endif
#if ENABLE_PROJECT_MENUS
    if (flow.is(AppStage::Menu)) return true;
#endif


    return false;
}

void Game::requestFullRedraw()
{
    redrawForeground(RedrawMode::Deferred);
}

void Game::redrawAllNow()
{
    redrawForeground(RedrawMode::Wake);
}

void Game::redrawForeground(RedrawMode mode)
{
    if (!initialized)
        return;
#if ENABLE_CAPTURE_GAME
    if (isCaptureActive())
    {
        minigame->redrawCapture();
        return;
    }
#endif
#if ENABLE_STAT_EVENTS
    if (redrawStatEvents()) return;
#endif
    if (mode == RedrawMode::Deferred) dirtySelect = true;
#if ENABLE_PROJECT_MENUS
    if (flow.is(AppStage::Menu)) { redrawProjectMenu(); return; }
#endif
#if ENABLE_SHOP
    if (flow.is(AppStage::Shop) && !shopPlayback) { redrawShop(); return; }
#endif
#if ENABLE_COLLECTION_CATALOG
    if (flow.is(AppStage::Collection))
    {
        if (collectionReplayPlaying) animations->requestFullRedraw();
        else if (!collectionCatalog->redraw()) enterRuntimeFatal();
        return;
    }
#endif
    if (mode == RedrawMode::Deferred)
    {
        animations->requestFullRedraw();
#if ENABLE_APPEARANCE_SELECTION
        if (flow.is(AppStage::Appearance)) appearanceSelection->requestFullRedraw();
#endif
        return;
    }

    const unsigned long now = millis();
#if ENABLE_APPEARANCE_SELECTION
    if (flow.is(AppStage::Appearance))
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
    if (flow.is(AppStage::Appearance))
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

bool Game::hasTransientAnimation() const
{
#if ENABLE_STAT_EVENTS
    if (statEventPageCount || committedEvolution.speciesSlot) return true;
#endif
    return animations->isBusy();
}

bool Game::preventsIdleSleep() const
{
#if ENABLE_CAPTURE_GAME
    if (isCaptureActive() && minigame->captureClockRunning()) return true;
#endif
#if ENABLE_STAT_EVENTS
    if (statEventPageCount || committedEvolution.speciesSlot) return true;
#endif
#if ENABLE_INSTRUCTION_RESPONSE
    if (capturesInstructionInput()) return true;
#endif
    return animations->hasFinitePlayback();
}

#if ENABLE_CAPTURE_GAME
bool Game::isCaptureActive() const
{
    return flow.is(AppStage::Minigame) && minigame->capturesScreen();
}

void Game::OnCaptureCancel()
{
    if (isCaptureActive()) minigame->reset();
}
#endif

void Game::stopPlaybackForSleep()
{
    animations->cancelAll();
    petBehaviorRuntime->interruptPendingActionPlayback();
    synchronizePlaybackClock();
}
void Game::startBatteryAnimation()
{
    if (!initialized)
        return;
#if ENABLE_INSTRUCTION_RESPONSE
    if (capturesInstructionInput())
    {
        minigame->reset();
        enterCommand();
        layout->begin();
        synchronizePlaybackClock();
    }
#endif
#if ENABLE_SHOP
    if (flow.is(AppStage::Shop)) shopPlayback = false;
#endif
    evolution->cancel();
    flow.endEvolution();
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
#if ENABLE_STAT_EVENTS
    if (isRoundComplete() || statEventPageCount || committedEvolution.speciesSlot) return;
#endif
    if (initialized && pet.isDead()) { OnDeathKey(); return; }
    if (pet.pendingActionOutcome() != UINT8_MAX) return;
    // Menu selection does not mutate pet values. Keep it responsive while
    // historical days settle; minigame decisions still wait for catch-up.
    if (!initialized || (hasPendingTime() && !flow.allowsMenuNavigation()))
        return;
    switch (flow.stage())
    {
#if ENABLE_PROJECT_MENUS
    case AppStage::Menu:
    {
        if (!projectMenu.move(direction == KeyDirection::Right)) enterRuntimeFatal();
        return;
    }
#endif
#if ENABLE_SHOP
    case AppStage::Shop:
    {
        if (!shopPlayback && !shopEvolutionPending && !shop.move(direction == KeyDirection::Right)) enterRuntimeFatal();
        return;
    }
#endif
#if ENABLE_COLLECTION_CATALOG
    case AppStage::Collection:
    {
        if (collectionReplayPlaying) return;
        if (!collectionCatalog->move(direction == KeyDirection::Right)) enterRuntimeFatal();
        return;
    }
#endif
#if ENABLE_APPEARANCE_SELECTION
    case AppStage::Appearance:
    {
        if (direction == KeyDirection::Left)
            appearanceSelection->onLeft();
        else
            appearanceSelection->onRight();
        return;
    }
#endif

#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE || ENABLE_CAPTURE_GAME
    case AppStage::Minigame:
    {
        if (direction == KeyDirection::Left)
            minigame->onLeft();
        else
            minigame->onRight();
        return;
    }
#endif

    case AppStage::Command:
    {
        if (direction == KeyDirection::Left)
            commands->prev();
        else
            commands->next();
        dirtySelect = true;
        return;
    }
    default:
        return;
    }
}

void Game::OnConfirmKey()
{
#if ENABLE_STAT_EVENTS
    if (statEventPageCount || committedEvolution.speciesSlot) return;
    if (initialized && isRoundComplete()) { resetPet(); return; }
#endif
    if (initialized && pet.isDead()) { OnDeathKey(); return; }
    if (pet.pendingActionOutcome() != UINT8_MAX) return;
    // Command playback itself postpones day boundaries. Keep replacement
    // responsive while it plays; idle RTC catch-up still precedes new actions.
    if (!initialized || (hasPendingTime() &&
        !(flow.is(AppStage::Command) && animations->isBusy())))
        return;
    switch (flow.stage())
    {
#if ENABLE_PROJECT_MENUS
    case AppStage::Menu:
    {
        confirmProjectMenu();
        return;
    }
#endif
#if ENABLE_SHOP
    case AppStage::Shop:
    {
        if (shopPlayback || shopEvolutionPending) return;
        const uint32_t previousDays = pet.stageDays();
        AssetData::AnimationRef animation = {};
        if (!shop.confirm(animation)) { enterRuntimeFatal(); return; }
        if (enterDeathIfNeeded()) return;
        shopEvolutionPending = pet.stageDays() != previousDays;
        if (!shop.active())
        {
            leaveStaticForeground();
            refreshBaseAnimation();
            requestFullRedraw();
        }
        else if (animation.valid())
        {
            const Animation use = Animation::complete(animation);
            if (animations->replace(AnimationSequence(&use, 1)) != PlaybackResult::Accepted)
                enterRuntimeFatal();
            else
            {
                animations->requestFullRedraw();
                shopPlayback = true;
            }
        }
        else if (shopEvolutionPending)
            finishShopPlayback();
        return;
    }
#endif
#if ENABLE_COLLECTION_CATALOG
    case AppStage::Collection:
    {
        if (collectionReplayPlaying)
        {
            collectionReplayPlaying = false;
            animations->cancelAll();
            if (!collectionCatalog->redraw()) enterRuntimeFatal();
            return;
        }
        if (!collectionCatalog->confirm()) { enterRuntimeFatal(); return; }
        AssetData::AnimationRef replay = {};
        uint8_t version = 0;
        if (collectionCatalog->takeReplay(replay, version))
        {
            Animation animation = Animation::complete(replay);
            animation.versionIndex = version;
            if (animations->replace(AnimationSequence(&animation, 1)) != PlaybackResult::Accepted)
            { enterRuntimeFatal(); return; }
            collectionReplayPlaying = true;
            return;
        }
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
            }
#if ENABLE_PROJECT_MENUS
            if (projectMenu.active())
            {
                flow.enter(AppStage::Menu);
                redrawProjectMenu();
                return;
            }
#endif
            leaveStaticForeground();
            requestFullRedraw();
        }
        return;
    }
#endif
#if ENABLE_APPEARANCE_SELECTION
    case AppStage::Appearance:
    {
        uint8_t selectedOutfit = 0;
        bool requiresUnlock = false;
        const bool confirmed = appearanceSelection->onConfirm(selectedOutfit, requiresUnlock);
        if (!appearanceSelection->isActive()) enterCommand();
        if (confirmed)
        {
            const AppearanceChangeResult result =
                appearanceChanges.applyOutfit(selectedOutfit, requiresUnlock);
            if (enterDeathIfNeeded()) return;
            if (result == AppearanceChangeResult::Applied)
            {
                appearanceSelection->exit();
                enterCommand();
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

#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE || ENABLE_CAPTURE_GAME
    case AppStage::Minigame:
    {
        const uint32_t previousDays = pet.stageDays();
        minigame->onConfirm();
        cheatEvolutionPending |= pet.stageDays() != previousDays;
        return;
    }
#endif

    case AppStage::Command:
    {
        const uint32_t previousDays = pet.stageDays();
        const CommandResult result = commandExecutor->execute(*commands);
        if (result.kind == CommandResultKind::UserAction)
            cheatEvolutionPending |= pet.stageDays() != previousDays;
        handleCommandResult(result);
        synchronizePlaybackClock();
        return;
    }
    default:
        return;
    }
}

#if ENABLE_PROJECT_MENUS
void Game::confirmProjectMenu()
{
    ProjectMenuSession::Entry entry;
    if (!projectMenu.confirm(entry))
    {
        enterRuntimeFatal();
        return;
    }
    if (!projectMenu.active())
    {
        finishProjectMenu();
        return;
    }
    CommandResult result;
    const uint32_t previousDays = pet.stageDays();
    if (entry.kind == ProjectMenuSession::Kind::UserAction)
        result = commandExecutor->executeUserAction(static_cast<uint8_t>(entry.target));
    else if (entry.kind == ProjectMenuSession::Kind::SystemCommand)
        result = commandExecutor->executeSystemCommand(static_cast<RuntimeSystemCommandId>(entry.target));
    else
        return;
    if (result.kind == CommandResultKind::UserAction)
        cheatEvolutionPending |= pet.stageDays() != previousDays;

    if (result.kind == CommandResultKind::Collection || result.kind == CommandResultKind::None)
    {
        handleCommandResult(result);
        return;
    }
    projectMenu.close();
    enterCommand();
    handleCommandResult(result);
    requestFullRedraw();
    synchronizePlaybackClock();
}

void Game::OnProjectMenuBack()
{
    if (!isProjectMenuActive()) return;
    if (!projectMenu.back())
        enterRuntimeFatal();
    else if (!projectMenu.active())
        finishProjectMenu();
}

void Game::finishProjectMenu()
{
    leaveStaticForeground();
    refreshBaseAnimation();
    requestFullRedraw();
}
#endif

bool Game::foregroundHoldsPetClock() const
{
#if ENABLE_CAPTURE_GAME
    if (isCaptureActive()) return true;
#endif
    return flow.holdsPetClock();
}

void Game::synchronizePlaybackClock()
{
    if (pet.pendingActionOutcome() != UINT8_MAX)
    {
        petClock.pause(latestRtcSeconds);
        return;
    }
#if ENABLE_CAPTURE_GAME
    if (isCaptureActive())
    {
        petClock.pause(latestRtcSeconds);
        return;
    }
#endif
#if ENABLE_STAT_EVENTS
    if (isRoundComplete() || statEventPageCount || committedEvolution.speciesSlot)
    {
        petClock.pause(latestRtcSeconds);
        return;
    }
#endif
    if (!(flow.is(AppStage::Command) || flow.is(AppStage::Appearance) || flow.is(AppStage::Evolution))
#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE || ENABLE_CAPTURE_GAME
        && !(flow.is(AppStage::Minigame) && minigame->pausesPetTime())
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

#if ENABLE_PROJECT_MENUS || ENABLE_SHOP || ENABLE_COLLECTION_CATALOG
void Game::prepareStaticForeground()
{
    petClock.pause(latestRtcSeconds);
    animations->cancelAll();
}

void Game::leaveStaticForeground()
{
    enterCommand();
    petClock.resume(latestRtcSeconds);
}
#endif

void Game::handleSystemCommandRequest(const CommandResult &result)
{
#if ENABLE_PROJECT_MENUS
    if (result.kind == CommandResultKind::Menu)
    {
        prepareStaticForeground();
        flow.enter(AppStage::Menu);
        if (!projectMenu.open(result.menuSlot)) enterRuntimeFatal();
        return;
    }
#endif
    if (result.kind == CommandResultKind::AdvanceDay)
    {
        if (runtime.behavior.manualDays) settlePetDays(1);
        return;
    }
#if ENABLE_SHOP
    if (result.kind == CommandResultKind::Shop || result.kind == CommandResultKind::Inventory)
    {
        prepareStaticForeground();
        if (!shop.start(result.kind == CommandResultKind::Inventory)) enterRuntimeFatal();
        else flow.enter(AppStage::Shop);
        return;
    }
#endif
#if ENABLE_COLLECTION_CATALOG
    if (result.kind == CommandResultKind::Collection)
    {
        prepareStaticForeground();
        if (!collectionCatalog->start()) enterRuntimeFatal();
        else flow.enter(AppStage::Collection);
        return;
    }
#endif

#if ENABLE_COMMAND_OUTFIT
    if (result.kind == CommandResultKind::Outfit)
    {
        if (appearanceSelection->start(pet.speciesSlot(), pet.outfitSlot(),
                                       pet.outfitUnlockMask()))
        {
            flow.enter(AppStage::Appearance);
            animations->cancelAll();
            animations->requestFullRedraw();
        }
        return;
    }
#endif

#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE || ENABLE_CAPTURE_GAME
#if ENABLE_CAPTURE_GAME
    if (result.kind == CommandResultKind::CaptureGame)
    {
        if (minigame->startCapture(renderer, resources, *this))
        {
            capturePet.restoreState(pet.persistentState());
            captureChanged = false;
            petClock.pause(latestRtcSeconds);
            flow.enter(AppStage::Minigame);
        }
        else
        {
            resources.recordAssetDataErrorResource("/capture.data");
            enterRuntimeFatal();
        }
        return;
    }
#endif
    if (result.kind == CommandResultKind::Instruction ||
        result.kind == CommandResultKind::Predict || result.kind == CommandResultKind::Guess)
    {
#if ENABLE_INSTRUCTION_RESPONSE
        if (result.kind == CommandResultKind::Instruction)
        {
#if ENABLE_TRAINING_COURSES
            const auto &training = runtime.instruction.trainingSettings;
            if (training.mode != TrainingMode::None && training.gateSlot != UINT8_MAX &&
                pet.customStat(training.gateSlot) != training.gateValue)
                return;
#endif
            petClock.pause(latestRtcSeconds);
            minigame->startInstruction(renderer, resources, runtime.instruction, runtime.playback);
            flow.enter(AppStage::Minigame);
            return;
        }
#endif
#if ENABLE_COMMAND_PREDICT
        if (result.kind == CommandResultKind::Predict)
        {
            if (minigame->startPredict()) flow.enter(AppStage::Minigame);
            return;
        }
#endif
#if ENABLE_GUESS_GAME
        minigame->startGuessItem();
        flow.enter(AppStage::Minigame);
#endif
        return;
    }
#endif
}

void Game::cancelForeground()
{
#if ENABLE_STAT_EVENTS
    clearStatEventPresentation();
#endif
#if ENABLE_PROJECT_MENUS
    projectMenu.close();
#endif
#if ENABLE_SHOP
    shop.close();
    shopPlayback = false;
    shopEvolutionPending = false;
#endif
    animations->cancelAll();
    evolution->cancel();
#if ENABLE_COLLECTION_CATALOG
    collectionCatalog->exit();
    collectionReplayPlaying = false;
#endif
#if ENABLE_APPEARANCE_SELECTION
    appearanceSelection->exit();
#endif
#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE || ENABLE_CAPTURE_GAME
    minigame->reset();
#endif
    enterCommand();
}

void Game::enterCommand()
{
    flow.enter(AppStage::Command);
    dirtySelect = true;
}

#if ENABLE_INSTRUCTION_RESPONSE
bool Game::capturesInstructionInput() const { return flow.is(AppStage::Minigame) && minigame->capturesInstructionInput(); }
void Game::instructionInput(uint8_t held, uint8_t pressed, uint32_t timestamp)
{
    if (capturesInstructionInput()) minigame->instructionInput(held, pressed, timestamp);
}
void Game::instructionPending(bool pending, uint32_t at, uint8_t raw)
{
    if (capturesInstructionInput()) minigame->instructionPending(pending, at, raw);
}
#endif
