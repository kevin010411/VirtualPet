#ifndef GAME_H
#define GAME_H

#include <Arduino.h>
#include "presentation/application/AppFlowController.h"
#include "animation/domain/Animation.h"
#include "appearance/ports/AppearanceLoader.h"

class AnimationController;
class CommandController;
class CommandExecutor;
struct CommandResult;
class LayoutRenderer;
class MinigameController;
class AppearanceSelectionController;
class Pet;
class PetActionController;
class PetStorage;
class Renderer;

class Game
{
public:
    Game(Pet &pet, PetStorage &petStorage, Renderer &renderer, AppearanceLoader &appearanceLoader);
    ~Game();

    void setup_game();
    void loop_game();
    void requestFullRedraw();
    void setRendererAssetAppearance(const char *speciesCode, const char *outfitCode);
    bool saveNow();
    bool startStartupAnimation();
    void startBatteryAnimation();
    void updateBatteryAnimation(unsigned long now);

    void OnLeftKey();
    void OnRightKey();
    void OnConfirmKey();
    void resetPet();

private:
    static constexpr unsigned long gameTick = 2000;

    Pet &pet;
    PetStorage &petStorage;
    Renderer &renderer;
    AppearanceLoader &appearanceLoader;
    AppFlowController flow;
    PetActionController *petActions;
    AnimationController *animations;
    CommandExecutor *commandExecutor;
    CommandController *commands;
    LayoutRenderer *layout;
    AppearanceSelectionController *appearanceSelection;
#if ENABLE_GUESS_ITEM_GAME
    MinigameController *minigame;
#endif

    unsigned long last_tick_time = 0;
    long environmentCooldown = 0;
    bool dirtySelect = true;
    bool pendingEvolution = false;
    char defaultSpeciesCode[9] = "dino";
    char defaultOutfitCode[9] = "base";
    char pendingEvolutionSpeciesCode[9] = {};
    char pendingEvolutionOutfitCode[9] = {};

    void refreshBaseAnimation();
    AnimationId currentBaseAnimation() const;
    void handleCommandResult(const CommandResult &result, int selectedSlot);
    void completeFirstLaunchIfNeeded(AppCommandId commandId);
    void maybeTickPet(unsigned long elapsed);
    bool completePendingEvolutionIfReady();
    void handleHealthyDaysEvolution();
    bool beginEvolutionAnimation(const AppearanceSelection &selection);
    bool isFirstLaunchSelectionPending() const;
    bool startFirstLaunchRequiredCommand();
    bool beginStartupAnimation();
    void enterFirstLaunch();
    void enterCommand();
};

#endif // GAME_H
