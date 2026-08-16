#ifndef GAME_H
#define GAME_H

#include <Arduino.h>
#include <memory>
#include "presentation/application/AppFlowController.h"
#include "animation/domain/Animation.h"
#include "appearance/ports/AppearanceLoader.h"
#include "pet_behavior/domain/PetBehaviorContract.h"

class AnimationController;
class CommandController;
class CommandExecutor;
struct CommandResult;
class LayoutRenderer;
class MinigameController;
class AppearanceSelectionController;
class Pet;
class PetActionController;
class PetBehaviorRuntime;
class PetStorage;
class Renderer;

class Game
{
public:
    Game(Pet &pet, PetStorage &petStorage, Renderer &renderer, AppearanceLoader &appearanceLoader);
    ~Game();

    Game(const Game &) = delete;
    Game &operator=(const Game &) = delete;

    bool setup_game();
    bool prepare_game();
    bool finish_setup_game();
    void loop_game();
    void requestFullRedraw();
    void redrawAllNow();
    void setRendererAssetAppearance(const char *speciesCode, const char *outfitCode);
    bool saveNow();
    bool startStartupAnimation();
    void startBatteryAnimation();
    void updateBatteryAnimation(unsigned long now);

    void OnLeftKey();
    void OnRightKey();
    void OnConfirmKey();
    bool resetPet();

private:
    static constexpr unsigned long gameTick = 2000;

    Pet &pet;
    PetStorage &petStorage;
    Renderer &renderer;
    AppearanceLoader &appearanceLoader;
    AppFlowController flow;
    PetBehaviorConfig petBehaviorConfig = {};
    std::unique_ptr<PetActionController> petActions;
    std::unique_ptr<AnimationController> animations;
    std::unique_ptr<PetBehaviorRuntime> petBehaviorRuntime;
    std::unique_ptr<CommandExecutor> commandExecutor;
    std::unique_ptr<CommandController> commands;
    std::unique_ptr<LayoutRenderer> layout;
#if ENABLE_APPEARANCE_SELECTION
    std::unique_ptr<AppearanceSelectionController> appearanceSelection;
#endif
#if ENABLE_GUESS_GAME
    std::unique_ptr<MinigameController> minigame;
#endif

    unsigned long last_tick_time = 0;
    bool dirtySelect = true;
    bool pendingEvolution = false;
    bool initialized = false;
    bool petBehaviorLoadingFailed = false;
    bool petBehaviorLoaded = false;
    bool setupPrepared = false;
    bool initialStateLoadingFailed = false;
    const char *startupConfigError = nullptr;
    char pendingEvolutionSpeciesCode[9] = {};
    char pendingEvolutionOutfitCode[9] = {};

    void refreshBaseAnimation();
    void syncActionLayoutWithAnimationQueue();
    void handleCommandResult(const CommandResult &result, int selectedSlot);
    void completeFirstLaunchIfNeeded(AppCommandId commandId);
    bool loadInitialPetState(bool allowSavedState, bool showError = true);
    void maybeTickPet();
    bool completePendingEvolutionIfReady();
    void handleEvolution();
    bool beginEvolutionAnimation(const AppearanceSelection &selection);
    bool isFirstLaunchSelectionPending() const;
    bool startFirstLaunchRequiredCommand();
    bool beginStartupAnimation();
    void enterFirstLaunch();
    void enterCommand();
};

#endif // GAME_H
