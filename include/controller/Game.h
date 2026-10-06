#ifndef GAME_H
#define GAME_H

#include <Arduino.h>
#include <memory>
#include "controller/AppFlowController.h"
#include "animation/Animation.h"
#include "appearance/AppearanceLoader.h"
#include "appearance/EvolutionController.h"
#include "appearance/AppearanceChangeController.h"
#include "pet/PetSaveController.h"
#include "resources/RuntimeContractLoader.h"
#include "pet/PetSession.h"

class AnimationController;
class CommandController;
class CommandExecutor;
struct CommandResult;
class LayoutRenderer;
class MinigameController;
class AppearanceSelectionController;
class Pet;
class PetBehaviorRuntime;
class PetStorage;
class Renderer;

class Game : private EvolutionHost, private PetSessionHost
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
    bool setStageDaysForCheat(uint32_t value);

private:
    enum class KeyDirection : uint8_t
    {
        Left,
        Right,
    };

    enum class RuntimeLoadState : uint8_t
    {
        Unloaded,
        Ready,
        Failed,
    };

    static constexpr unsigned long gameTick = 2000;

    Pet &pet;
    Renderer &renderer;
    AppearanceLoader &appearanceLoader;
    AppFlowController flow;
    PetBehaviorConfig petBehaviorConfig = {};
    PetSaveController petSaves;
    AppearanceChangeController appearanceChanges;
    std::unique_ptr<AnimationController> animations;
    std::unique_ptr<EvolutionController> evolution;
    std::unique_ptr<PetBehaviorRuntime> petBehaviorRuntime;
    PetSession petSession;
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
    bool cheatEvolutionPending = false;
    bool pendingFirstStartCompletion = false;
    bool initialized = false;
    RuntimeLoadState runtimeLoadState = RuntimeLoadState::Unloaded;
    bool configureActiveAppearance(uint8_t speciesSlot, uint8_t outfitSlot) override;
    bool activateLoadedAppearance(uint8_t speciesSlot, uint8_t outfitSlot) override;
    bool refreshOutfitUnlockMask(bool initialize);
    AppearanceChangeResult enterSpecies(uint8_t speciesSlot, uint8_t entryOutfitSlot) override;
    void refreshBaseAnimation() override;
    PlaybackTickResult tickPlayback(unsigned long now);
    bool completePlaybackTick(const PlaybackTickResult &playbackResult);
    void routeDirectionKey(KeyDirection direction);
    void handleCommandResult(const CommandResult &result);
    void maybeTickPet();
    EvolutionResult handleEvolution();
    void handleEvolutionResult(EvolutionResult result);
    // State-only: prepare runs before display initialization.
    void enterFatalState();
    void enterRuntimeFatal();
    bool beginStartupAnimation();
    void handlePlaybackResult(PlaybackResult playbackResult);
    void completeFirstStartIfReady(const PlaybackTickResult &playbackResult);
    void enterCommand();
};

#endif // GAME_H
