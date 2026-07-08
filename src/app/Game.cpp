#include "app/Game.h"

#include <string.h>
#include "app/AnimationController.h"
#include "app/CommandController.h"
#include "app/CommandExecutor.h"
#include "app/LayoutRenderer.h"
#include "app/MinigameController.h"
#include "app/AppearanceSelectionController.h"
#include "app/PetActionController.h"
#include "domain/Pet.h"
#include "render/Renderer.h"
#include "storage/PetStorage.h"

Game::Game(Pet &petRef, PetStorage &petStorageRef, Renderer &rendererRef, AppearanceLoader &appearanceLoaderRef)
    : pet(petRef),
      petStorage(petStorageRef),
      renderer(rendererRef),
      appearanceLoader(appearanceLoaderRef),
      petActions(new PetActionController(pet, petStorage, renderer, appearanceLoader)),
      animations(new AnimationController(renderer)),
      commandExecutor(new CommandExecutor(*petActions, *animations)),
      commands(new CommandController(*commandExecutor)),
      layout(new LayoutRenderer(renderer, *commands)),
      appearanceSelection(new AppearanceSelectionController(renderer, appearanceLoader))
#if ENABLE_GUESS_ITEM_GAME
      ,
      minigame(new MinigameController(*petActions, *animations))
#endif
{
}

Game::~Game()
{
#if ENABLE_GUESS_ITEM_GAME
    delete minigame;
#endif
    delete appearanceSelection;
    delete layout;
    delete commands;
    delete commandExecutor;
    delete animations;
    delete petActions;
}

void Game::setup_game()
{
    commands->resetSelection();
    animations->setup(currentBaseAnimation());
    layout->begin();
    layout->drawAll();

    dirtySelect = true;
    environmentCooldown = 0;
    pendingEvolution = false;
    pendingEvolutionSpeciesCode[0] = '\0';
    pendingEvolutionOutfitCode[0] = '\0';
    last_tick_time = millis();
    appearanceSelection->exit();
#if ENABLE_GUESS_ITEM_GAME
    minigame->reset();
#endif

    petActions->loadOrDefault(defaultSpeciesCode, defaultOutfitCode);
    petActions->applySpeciesForHealthyDays();
    renderer.setAssetAppearance(petActions->speciesCode(), petActions->outfitCode());
    renderer.reloadManifest();
    refreshBaseAnimation();

    enterCommand();
}

void Game::loop_game()
{
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

    if (appearanceSelection->isActive())
    {
        appearanceSelection->render(now);
        if (dirtySelect)
        {
            layout->drawSelection();
            dirtySelect = false;
        }
        return;
    }

    animations->render(now);
    if (layout->isActionActive() && !animations->hasAnimationForOwner(AnimationOwner::Command))
        layout->endAction();

    if (dirtySelect)
    {
        layout->drawSelection();
        dirtySelect = false;
    }
}

void Game::requestFullRedraw()
{
    dirtySelect = true;
    animations->requestFullRedraw();
}

void Game::setRendererAssetAppearance(const char *speciesCode, const char *outfitCode)
{
    if (pet.setSpeciesCode(speciesCode))
    {
        strncpy(defaultSpeciesCode, pet.speciesCode(), sizeof(defaultSpeciesCode) - 1);
        defaultSpeciesCode[sizeof(defaultSpeciesCode) - 1] = '\0';
    }

    if (pet.setOutfitCode(outfitCode))
    {
        strncpy(defaultOutfitCode, pet.outfitCode(), sizeof(defaultOutfitCode) - 1);
        defaultOutfitCode[sizeof(defaultOutfitCode) - 1] = '\0';
    }

    renderer.setAssetAppearance(defaultSpeciesCode, defaultOutfitCode);
    renderer.reloadManifest();
    refreshBaseAnimation();
    animations->markDirty();
}

bool Game::saveNow()
{
    return petActions->saveNow();
}

bool Game::startStartupAnimation()
{
    if (!flow.requestStartup())
        return false;

    return beginStartupAnimation();
}

void Game::startBatteryAnimation()
{
    animations->startBatteryAnimation();
}

void Game::updateBatteryAnimation(unsigned long now)
{
    animations->updateBatteryAnimation(now);
}

void Game::OnLeftKey()
{
    if (appearanceSelection->isActive())
    {
        appearanceSelection->onLeft();
        return;
    }

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
    if (appearanceSelection->isActive())
    {
        appearanceSelection->onRight();
        return;
    }

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
    commandExecutor->begin(commandId);
    const bool executed = commands->executeCurrent();
    handleCommandResult(commandExecutor->complete(executed), selectedSlot);
}

void Game::resetPet()
{
    petActions->reset();
    pendingEvolution = false;
    pendingEvolutionSpeciesCode[0] = '\0';
    pendingEvolutionOutfitCode[0] = '\0';
    petActions->applySpeciesForHealthyDays();
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
}

void Game::refreshBaseAnimation()
{
    animations->setBaseAnimation(currentBaseAnimation());
}

AnimationId Game::currentBaseAnimation() const
{
    const AnimationId candidateAnimation = petActions->currentAnimation();
    if (animations->hasAnimation(candidateAnimation))
        return candidateAnimation;

    if (candidateAnimation != AnimationId::Idle && animations->hasAnimation(AnimationId::Idle))
        return AnimationId::Idle;

    return candidateAnimation;
}

void Game::handleCommandResult(const CommandResult &result, int selectedSlot)
{
    if (!result.executed)
        return;

    layout->enterAction(result.layoutId, selectedSlot);

    if (result.requestedOutfit)
    {
        if (appearanceSelection->start(petActions->speciesCode(), petActions->outfitCode()))
        {
            animations->clearByOwner(AnimationOwner::Command);
            animations->requestFullRedraw();
        }
        return;
    }

    if (result.requestedSpecies)
    {
        if (appearanceSelection->startSpecies(petActions->speciesCode()))
        {
            animations->clearByOwner(AnimationOwner::Command);
            animations->requestFullRedraw();
        }
        return;
    }

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
    return ENABLE_FIRST_LAUNCH_SELECTION && !petActions->isFirstLaunchComplete();
}

bool Game::startFirstLaunchRequiredCommand()
{
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

    completeFirstLaunchIfNeeded(flow.firstLaunchRequiredCommand());
    return false;
}

void Game::maybeTickPet(unsigned long elapsed)
{
    if (completePendingEvolutionIfReady())
        return;

    if (pendingEvolution)
        return;

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
        if (randValue < probability)
            petActions->getSick();

        if (petActions->dayPassed())
        {
            handleHealthyDaysEvolution();
            if (pendingEvolution)
                return;
        }
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

void Game::handleHealthyDaysEvolution()
{
    AppearanceSelection selection = {};
    if (!petActions->findSpeciesForHealthyDays(selection))
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
    if (!animations->hasAnimation(AnimationId::Start))
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
    const unsigned long startupDuration = max(
        gameTick,
        static_cast<unsigned long>(animations->frameCountFor(AnimationId::Start)) *
            animations->frameIntervalFor(AnimationId::Start));
    animations->queueAnimation(Animation(AnimationId::Start, startupDuration, true, AnimationOwner::System, AnimationPriority::Critical));
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
