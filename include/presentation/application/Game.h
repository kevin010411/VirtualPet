#ifndef GAME_H
#define GAME_H

#include <Arduino.h>
#include <memory>
#include "presentation/application/AppFlowController.h"
#include "animation/domain/Animation.h"
#include "appearance/ports/AppearanceLoader.h"
#include "pet_behavior/domain/RuntimeContractLoader.h"

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
    void setRendererAssetAppearance(uint8_t speciesSlot, uint8_t outfitSlot);
    bool saveNow();
    bool startStartupAnimation();
    bool hasTransientAnimation() const;
    void startBatteryAnimation();
    void endBatteryAnimation();
    void updateBatteryAnimation(unsigned long now);

    void OnLeftKey();
    void OnRightKey();
    void OnConfirmKey();
    bool resetPet();

private:
    enum class InitialPetStateResult : uint8_t
    {
        Failed,
        Fresh,
        Restored,
    };

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
    bool pendingFirstStartCompletion = false;
    bool initialized = false;
    bool petBehaviorLoadingFailed = false;
    bool petBehaviorLoaded = false;
    bool setupPrepared = false;
    bool initialStateLoadingFailed = false;
    const char *startupConfigError = nullptr;
#if ENABLE_DEBUG
    const char *startupDebugStage = nullptr;
#endif
    uint8_t pendingEvolutionSpeciesSlot = 0;
    uint8_t pendingEvolutionOutfitSlot = 0;

    bool configureActiveAppearance(uint8_t speciesSlot, uint8_t outfitSlot);
    bool resolveOutfitUnlockMask(bool initialize);
    bool refreshOutfitUnlockMask(bool initialize);
    bool enterSpecies(uint8_t speciesSlot, uint8_t entryOutfitSlot);
    void refreshBaseAnimation();
    void syncActionLayoutWithPlayback();
    void handleCommandResult(const CommandResult &result, int selectedSlot);
    void completeFirstLaunchIfNeeded(AppCommandId commandId);
    InitialPetStateResult loadInitialPetState(bool allowSavedState,
                                               bool showError = true);
    void maybeTickPet();
    bool completePendingEvolutionIfReady();
    void handleEvolution();
    bool beginEvolutionAnimation(const AppearanceSelection &selection);
    bool isFirstLaunchSelectionPending() const;
    bool startFirstLaunchRequiredCommand();
    bool beginStartupAnimation();
    void handlePlaybackResult(PlaybackResult playbackResult);
    void completeFirstStartIfReady(const PlaybackTickResult &playbackResult);
    void enterFirstLaunch();
    void enterCommand();
};

#endif // GAME_H
