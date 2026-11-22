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
#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE || ENABLE_CAPTURE_GAME
#include "controller/MinigameController.h"
#endif
#include "pet/PetClock.h"
#if ENABLE_STAT_EVENTS
#include "pet/PetStatEvents.h"
#endif
#if ENABLE_PROJECT_MENUS
#include "controller/ProjectMenuSession.h"
#endif
#if ENABLE_SHOP
#include "shop/ShopSession.h"
#endif

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
class AssetResources;
class CollectionCatalog;

class Game : private EvolutionHost, private PetSessionHost
#if ENABLE_CAPTURE_GAME
           , private CaptureOutcomeHost
#endif
#if ENABLE_GUESS_GAME
           , private MinigameOutcomeHost
#endif
{
public:
    Game(Pet &pet, PetStorage &petStorage, Renderer &renderer, AssetResources &resources, AppearanceLoader &appearanceLoader);
    ~Game();

    Game(const Game &) = delete;
    Game &operator=(const Game &) = delete;

    bool setup_game();
    bool prepare_game();
    bool finish_setup_game();
    void loop_game(bool confirmPending = false);
    void synchronizeTime(uint32_t rtcSeconds);
    bool hasPendingTime() const { return petClock.pending() && initialized; }
    bool isReady() const { return initialized; }
    bool isDead() const;
#if ENABLE_STAT_EVENTS
    bool isRoundComplete() const;
#endif
    void OnDeathKey();
    void requestFullRedraw();
    void redrawAllNow();
    bool saveNow();
    bool startStartupAnimation();
    bool hasTransientAnimation() const;
    bool preventsIdleSleep() const;
    void stopPlaybackForSleep();
    bool audioActive() const;
    bool isCollectionCatalogActive() const;
    void startBatteryAnimation();
    void endBatteryAnimation();
    void updateBatteryAnimation(unsigned long now);

#if ENABLE_INSTRUCTION_RESPONSE
    bool capturesInstructionInput() const;
    void instructionInput(uint8_t held, uint8_t pressed, uint32_t timestamp);
    void instructionPending(bool pending, uint32_t at, uint8_t raw);
#endif
    void OnLeftKey();
    void OnRightKey();
    void OnConfirmKey();
#if ENABLE_CAPTURE_GAME
    bool isCaptureActive() const;
    void OnCaptureCancel();
#endif
#if ENABLE_PROJECT_MENUS
    bool isProjectMenuActive() const { return flow.is(AppStage::Menu); }
    void OnProjectMenuBack();
#endif
    bool resetPet();
    bool setStageDaysForCheat(uint32_t value);

private:
#if ENABLE_CAPTURE_GAME
    bool onCaptureOutcome(CaptureAssets &assets, const CaptureAssets::Target *target,
                          const CollectionRoundStats &round, int32_t score,
                          CatchResult &result) override;
    Pet capturePet;
    bool captureChanged = false;
#endif

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

    static constexpr unsigned long startupSegmentMs = 2000;

    Pet &pet;
    Renderer &renderer;
    AssetResources &resources;
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
#if ENABLE_PROJECT_MENUS
    ProjectMenuSession projectMenu;
    void confirmProjectMenu();
    void redrawProjectMenu();
    void finishProjectMenu();
#endif
#if ENABLE_COLLECTION_CATALOG
    std::unique_ptr<CollectionCatalog> collectionCatalog;
    bool collectionReplayPlaying = false;
    bool collectionSavePending = false;
#endif
#if ENABLE_APPEARANCE_SELECTION
    std::unique_ptr<AppearanceSelectionController> appearanceSelection;
#endif
#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE || ENABLE_CAPTURE_GAME
    std::unique_ptr<MinigameController> minigame;
#endif

#if ENABLE_SHOP
    ShopSession shop;
    bool shopPlayback = false;
    bool shopEvolutionPending = false;
    bool prepareShopProgress(bool &changed) override { return shop.arrive(changed); }
    void redrawShop();
    void finishShopPlayback(bool restoreDisplay = false);
#endif
    PetClock petClock;
    uint32_t latestRtcSeconds = 0;
    bool clockStarted = false;
    // Settle one event boundary before skipping a stable catch-up interval.
    bool catchUpPrimed = false;
    bool dirtySelect = true;
    bool cheatEvolutionPending = false;
    bool pendingFirstStartCompletion = false;
    bool initialized = false;
#if ENABLE_STAT_EVENTS
    uint16_t statEventPages[kMaxStatEventPages] = {};
    AppearanceSelection committedEvolution = {};
    uint8_t statEventPageCount = 0;
    uint8_t statEventPage = 0;
    uint16_t statEventAnimationId = 0;
    uint16_t statEventPageDuration = 0;
    unsigned long statEventPageStarted = 0;
    bool playingCommittedEvolution = false;
    bool statEventsActive() const;
    bool settleStatMutation(const PetStatSnapshot &after, uint8_t actionSlot, bool daily);
    bool presentStatEvents(unsigned long now);
    bool redrawStatEvents();
    void clearStatEventPresentation();
#endif
#if ENABLE_DEATH
    bool deathPresented = false;
    bool deathSavePending = false;
    unsigned long deathSaveRetryAt = 0;
    bool enterDeathIfNeeded();
    void retryDeathSave();
#else
    bool enterDeathIfNeeded() { return isDead(); }
    void retryDeathSave() {}
#endif
    RuntimeLoadState runtimeLoadState = RuntimeLoadState::Unloaded;
    bool configureActiveAppearance(uint8_t speciesSlot, uint8_t outfitSlot) override;
    bool activateLoadedAppearance(uint8_t speciesSlot, uint8_t outfitSlot) override;
    bool refreshOutfitUnlockMask(bool initialize, bool persist = true);
    AppearanceChangeResult enterSpecies(uint8_t speciesSlot, uint8_t entryOutfitSlot) override;
    void refreshBaseAnimation() override;
    PlaybackTickResult tickPlayback(unsigned long now);
    bool continuePendingAction();
    void synchronizePlaybackClock();
    bool completePlaybackTick(const PlaybackTickResult &playbackResult);
    void routeDirectionKey(KeyDirection direction);
    void handleCommandResult(const CommandResult &result);
    enum class Settlement : uint8_t { UserAction, GuessOutcome, InstructionCompletion, DailyProgress };
    EvolutionResult settleMutation(Settlement source);
    void handleSystemCommandRequest(const CommandResult &result);
    // Returns clock consumption; blocked or fatal work consumes no backlog.
    uint32_t settlePetDays(uint32_t dueDays);
    EvolutionResult handleEvolution();
    void handleEvolutionResult(EvolutionResult result);
    // State-only: prepare runs before display initialization.
    void enterFatalState();
    void enterRuntimeFatal();
    bool beginStartupAnimation();
    void handlePlaybackResult(PlaybackResult playbackResult);
    void completeFirstStartIfReady(const PlaybackTickResult &playbackResult);
    void enterCommand();
    void cancelForeground();
    bool updateCollectionProgress(bool immediate = false);
#if ENABLE_COLLECTION_CATALOG
    bool finishCollectionSave(PetSaveResult result);
#endif
#if ENABLE_GUESS_GAME
    void onGuessOutcomeApplied() override;
#endif
};

#endif // GAME_H
