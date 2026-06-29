#ifndef GAME_H
#define GAME_H

#include <Arduino.h>
#include "app/AppFlowController.h"
#include "domain/Animation.h"
#include "storage/AppearanceLoader.h"

class AnimationController;
class CommandController;
class CommandExecutor;
struct CommandResult;
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
    AppearanceSelectionController *appearanceSelection;
#if ENABLE_GUESS_ITEM_GAME
    MinigameController *minigame;
#endif

    unsigned long last_tick_time = 0;
    long environmentCooldown = 0;
    bool dirtySelect = true;
    char defaultSpeciesCode[9] = "dino";
    char defaultOutfitCode[9] = "base";

    void refreshBaseAnimation();
    AnimationId currentBaseAnimation() const;
    void draw_all_layout();
    void draw_select_layout();
    void handleCommandResult(const CommandResult &result);
    void completeFirstLaunchIfNeeded(AppCommandId commandId);
    void maybeTickPet(unsigned long elapsed);
    bool isFirstLaunchSelectionPending() const;
    bool startFirstLaunchRequiredCommand();
    bool beginStartupAnimation();
    void enterFirstLaunch();
    void enterCommand();
};

#endif // GAME_H
