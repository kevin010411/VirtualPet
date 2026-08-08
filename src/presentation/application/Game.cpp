#include "presentation/application/Game.h"

#include <string.h>
#include "animation/application/AnimationController.h"
#include "commands/application/CommandController.h"
#include "commands/application/CommandExecutor.h"
#include "custom_rules/domain/CustomRules.h"
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
      petBehaviorRuntime(std::make_unique<PetBehaviorRuntime>(petBehaviorConfig, *petActions, *animations)),
      commandExecutor(std::make_unique<CommandExecutor>(*petActions, *animations, customRules)),
      commands(std::make_unique<CommandController>(*commandExecutor)),
      layout(std::make_unique<LayoutRenderer>(renderer, *commands))
#if ENABLE_APPEARANCE_SELECTION
      ,
      appearanceSelection(std::make_unique<AppearanceSelectionController>(renderer, appearanceLoader))
#endif
#if ENABLE_GUESS_ITEM_GAME
      ,
      minigame(std::make_unique<MinigameController>(*petActions, *animations))
#endif
{
}

Game::~Game()
    = default;

bool Game::setup_game()
{
    initialized = false;
    if (petBehaviorLoadingFailed || !loadPetBehaviorContract(animations->sdCard(), petBehaviorConfig))
    {
        petBehaviorLoadingFailed = true;
        petBehaviorLoaded = false;
        renderer.showPetBehaviorLoadingError();
        return false;
    }
    petBehaviorLoaded = true;
    commands->resetSelection();
    animations->setup(petBehaviorConfig.idleAnimation);
    layout->begin();
    layout->drawAll();

    dirtySelect = true;
    environmentCooldown = 0;
    pendingEvolution = false;
    pendingEvolutionSpeciesCode[0] = '\0';
    pendingEvolutionOutfitCode[0] = '\0';
    last_tick_time = millis();
#if ENABLE_APPEARANCE_SELECTION
    appearanceSelection->exit();
#endif
#if ENABLE_GUESS_ITEM_GAME
    minigame->reset();
#endif

#if ENABLE_CUSTOM_RULES
#if ENABLE_DEBUG
    customRules.load(animations->sdCard(), &renderer.debugDisplay());
#else
    customRules.load(animations->sdCard());
#endif
#endif
    if (!loadInitialPetState(true))
        return false;

    // A successfully restored appearance is authoritative. Re-evaluating the
    // evolution table here can immediately replace a saved later-stage species
    // with an earlier wildcard/fallback match before the startup animation.
    // Evolution is still evaluated by handleEvolution() during normal ticks.
    renderer.setAssetAppearance(petActions->speciesCode(), petActions->outfitCode());
    renderer.reloadManifest();
    refreshBaseAnimation();

    initialized = true;
    enterCommand();
    return true;
}

void Game::loop_game()
{
    if (!initialized)
        return;

    const unsigned long now = millis();
    const unsigned long elapsed = now - last_tick_time;

    if (elapsed >= gameTick)
    {
        last_tick_time = now;
        animations->updateElapsed(elapsed);

        if (flow.isCommand() || flow.isMinigame())
            maybeTickPet(elapsed);

        if (flow.isStartup() && !animations->hasAnimationForOwner(AnimationOwner::System))
        {
            if (isFirstLaunchSelectionPending())
                enterFirstLaunch();
            else
                enterCommand();
        }
    }

#if ENABLE_GUESS_ITEM_GAME
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

    if (layout->isActionActive() && !animations->hasAnimationForOwner(AnimationOwner::Command))
    {
        refreshBaseAnimation();
        layout->endAction();
    }
    animations->render(now);
    syncActionLayoutWithAnimationQueue();

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
    animations->render(now);

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
        renderer.showPetBehaviorLoadingError();
        return;
    }
    if (!pet.setSpeciesCode(speciesCode) || !pet.setOutfitCode(outfitCode))
    {
        initialized = false;
        renderer.showInitPetNotExist();
        return;
    }

    renderer.setAssetAppearance(pet.speciesCode(), pet.outfitCode());
    renderer.reloadManifest();
    refreshBaseAnimation();
    animations->markDirty();
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

void Game::startBatteryAnimation()
{
    if (!initialized)
        return;
    animations->startBatteryAnimation();
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

#if ENABLE_GUESS_ITEM_GAME
    if (flow.isMinigame())
    {
        minigame->onLeft();
        return;
    }
#endif

    if (flow.isCommand() || flow.isStartup())
    {
        commands->next();
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

#if ENABLE_GUESS_ITEM_GAME
    if (flow.isMinigame())
    {
        minigame->onRight();
        return;
    }
#endif

    if (flow.isCommand() || flow.isStartup())
    {
        commands->prev();
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

#if ENABLE_GUESS_ITEM_GAME
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
        animations->clearByOwner(AnimationOwner::Command);
        if (petBehaviorRuntime->executeAction(behaviorButton.actionSlot))
        {
            refreshBaseAnimation();
            layout->enterAction(AnimationId::None, selectedSlot);
        }
        return;
    }
    commandExecutor->begin(commandId);
    const bool executed = commands->executeCurrent();
    handleCommandResult(commandExecutor->complete(executed), selectedSlot);
}

bool Game::resetPet()
{
    if (!petBehaviorLoaded)
        return false;
    initialized = false;
    if (!loadInitialPetState(false))
        return false;

    pendingEvolution = false;
    pendingEvolutionSpeciesCode[0] = '\0';
    pendingEvolutionOutfitCode[0] = '\0';
    petActions->applyEvolutionTarget();
    renderer.setAssetAppearance(petActions->speciesCode(), petActions->outfitCode());
    renderer.reloadManifest();
    animations->clearByOwner(AnimationOwner::Command);
    animations->clearByOwner(AnimationOwner::Minigame);
    animations->clearByOwner(AnimationOwner::System);
#if ENABLE_GUESS_ITEM_GAME
    minigame->reset();
#endif
    refreshBaseAnimation();
    animations->requestFullRedraw();
    petActions->saveNow();
    if (ENABLE_STARTUP_ANIMATION)
        startStartupAnimation();
    else if (isFirstLaunchSelectionPending())
        enterFirstLaunch();
    else
        enterCommand();
    initialized = true;
    return true;
}

void Game::refreshBaseAnimation()
{
    animations->setBaseAnimation(petBehaviorRuntime->baseAnimation());
}

void Game::syncActionLayoutWithAnimationQueue()
{
    layout->updateAction(animations->currentCommandAnimationId());
}

void Game::handleCommandResult(const CommandResult &result, int selectedSlot)
{
    if (!result.executed)
        return;

    layout->enterAction(animations->currentCommandAnimationId(), selectedSlot);

#if ENABLE_COMMAND_OUTFIT
    if (result.requestedOutfit)
    {
        if (appearanceSelection->start(petActions->speciesCode(), petActions->outfitCode()))
        {
            animations->clearByOwner(AnimationOwner::Command);
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
            animations->clearByOwner(AnimationOwner::Command);
            animations->requestFullRedraw();
        }
        return;
    }
#endif

#if ENABLE_GUESS_ITEM_GAME
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

bool Game::loadInitialPetState(bool allowSavedState)
{
    if (allowSavedState && petStorage.load(pet))
    {
        return true;
    }

    AppearanceSelection initialAppearance = {};
    if (!appearanceLoader.findInitialAppearance(initialAppearance))
    {
        strncpy(initialAppearance.speciesCode, APP_INITIAL_SPECIES, sizeof(initialAppearance.speciesCode) - 1);
        initialAppearance.speciesCode[sizeof(initialAppearance.speciesCode) - 1] = '\0';
        strncpy(initialAppearance.outfitCode, APP_INITIAL_OUTFIT, sizeof(initialAppearance.outfitCode) - 1);
        initialAppearance.outfitCode[sizeof(initialAppearance.outfitCode) - 1] = '\0';
    }

    pet.setDefaultState();
    petBehaviorRuntime->initializeStats();
    const bool applied = pet.setSpeciesCode(initialAppearance.speciesCode) &&
                         pet.setOutfitCode(initialAppearance.outfitCode);
    if (!applied)
        renderer.showInitPetNotExist();
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
            animations->clearByOwner(AnimationOwner::Command);
            animations->requestFullRedraw();
            return true;
        }
        break;
    case AppCommandId::ChangeSpecies:
        if (appearanceSelection->startSpecies(petActions->speciesCode()))
        {
            animations->clearByOwner(AnimationOwner::Command);
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

void Game::maybeTickPet(unsigned long elapsed)
{
    if (completePendingEvolutionIfReady())
        return;

    if (pendingEvolution)
        return;

    if (!animations->hasAnimationForOwner(AnimationOwner::Command) &&
        !animations->hasAnimationForOwner(AnimationOwner::Minigame) &&
        !animations->hasAnimationForOwner(AnimationOwner::System))
    {
        handleEvolution();
        if (pendingEvolution)
            return;
    }

    environmentCooldown -= static_cast<long>(elapsed);
    if (environmentCooldown <= 0)
    {
        petActions->decayEnvironment();
        environmentCooldown = random(3 * gameTick, 6 * gameTick);
    }

    if (!animations->hasAnimationForOwner(AnimationOwner::Command) &&
        !animations->hasAnimationForOwner(AnimationOwner::Minigame) &&
        !animations->hasAnimationForOwner(AnimationOwner::System))
    {
        const int probability = 1;
        const int randValue = random(1001);
        petActions->dayPassed();
        petBehaviorRuntime->applyDailyChanges();
        handleEvolution();
        if (pendingEvolution)
            return;

        if (randValue < probability)
            petActions->getSick();
    }

    refreshBaseAnimation();
    petActions->maybeSave();
}

bool Game::completePendingEvolutionIfReady()
{
    if (!pendingEvolution)
        return false;

    if (animations->hasAnimationForOwner(AnimationOwner::System))
        return false;

    const bool applied = petActions->applyAppearance(pendingEvolutionSpeciesCode, pendingEvolutionOutfitCode);
    pendingEvolution = false;
    pendingEvolutionSpeciesCode[0] = '\0';
    pendingEvolutionOutfitCode[0] = '\0';
    if (applied)
    {
        refreshBaseAnimation();
        animations->requestFullRedraw();
    }
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

    const unsigned long evolutionDuration = max(
        gameTick,
        static_cast<unsigned long>(animations->frameCountFor(AnimationId::Evolution)) *
            animations->frameIntervalFor(AnimationId::Evolution));
    animations->queueAnimation(Animation(AnimationId::Evolution, evolutionDuration, true, AnimationOwner::System, AnimationPriority::Critical));
    animations->markDirty();
    return true;
}

bool Game::beginStartupAnimation()
{
#if ENABLE_STARTUP_ANIMATION
    const bool hasIntro = animations->hasAnimation(AnimationId::StartIntro);
    const bool hasSpeciesStart = animations->hasAnimation(AnimationId::Start);
    if (!hasIntro && !hasSpeciesStart)
    {
        if (isFirstLaunchSelectionPending())
            enterFirstLaunch();
        else
            enterCommand();
        return false;
    }

    flow.requestStartup();
    animations->clearByOwner(AnimationOwner::Command);
    animations->clearByOwner(AnimationOwner::Minigame);
    animations->clearByOwner(AnimationOwner::System);

    if (hasIntro)
    {
        const unsigned long introDuration = max(
            gameTick,
            static_cast<unsigned long>(animations->frameCountFor(AnimationId::StartIntro)) *
                animations->frameIntervalFor(AnimationId::StartIntro));
        animations->queueAnimation(Animation(AnimationId::StartIntro, introDuration, true, AnimationOwner::System, AnimationPriority::Critical));
    }

    if (hasSpeciesStart)
    {
        const unsigned long startupDuration = max(
            gameTick,
            static_cast<unsigned long>(animations->frameCountFor(AnimationId::Start)) *
                animations->frameIntervalFor(AnimationId::Start));
        animations->queueAnimation(Animation(AnimationId::Start, startupDuration, true, AnimationOwner::System, AnimationPriority::Critical));
    }

    animations->markDirty();
    return true;
#else
    if (isFirstLaunchSelectionPending())
        enterFirstLaunch();
    else
        enterCommand();
    return false;
#endif
}

void Game::enterFirstLaunch()
{
    flow.beginFirstLaunch();
#if ENABLE_GUESS_ITEM_GAME
    minigame->reset();
#endif
    animations->clearByOwner(AnimationOwner::Command);
    animations->clearByOwner(AnimationOwner::Minigame);
    dirtySelect = true;
    startFirstLaunchRequiredCommand();
    animations->requestFullRedraw();
}

void Game::enterCommand()
{
    flow.enterCommand();
    dirtySelect = true;
}
