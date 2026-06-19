#ifndef GAME_H
#define GAME_H

#include <Arduino.h>
#include <deque>
#include "storage/AppearanceLoader.h"
#include "app/AppProfile.h"
#include "domain/Animation.h"
#if ENABLE_GUESS_ITEM_GAME
#include "minigame/GuessItemGame.h"
#endif
#include "render/Renderer.h"

class Adafruit_ST7735;
class Pet;
class PetStorage;
class Renderer;

class Game
#if ENABLE_GUESS_ITEM_GAME
    : public GuessItemGameHost
#endif
{
public:
    Game(Pet &pet, PetStorage &petStorage, Renderer &renderer, AppearanceLoader &appearanceLoader);
    ~Game();

    void setup_game();
    void loop_game();
    void requestFullRedraw();
    void setRendererAssetAppearance(const char *speciesCode, const char *outfitCode);
    bool saveNow();
    bool showBatteryScreen();
    void startBatteryAnimation();
    void updateBatteryAnimation(unsigned long now);

    String NowCommand();
    void OnLeftKey();
    void OnRightKey();
    void OnConfirmKey();
    void resetPet();

    void queueAnimation(const Animation &animation);
    void clearAnimationsByOwner(AnimationOwner owner);
    void markAnimationDirty();
    void changePetMood(int delta);
    bool hasAnimation(AnimationId id) const;
    bool hasAnimationForOwner(AnimationOwner owner) const;

private:
    static constexpr unsigned long gameTick = 2000;
    static constexpr unsigned long frameIntervalSlow = 600;
    static constexpr uint8_t savePeriodTicks = 2;
    static constexpr unsigned int environmentDecayAmount = 5;
    static constexpr int maxFortune = 11;
    static constexpr size_t maxOutfitOptions = 8;

    enum class Command
    {
        NoOp,
        FeedPet,
        Predict,
        Gift,
        Medicine,
        Shower,
        HaveFun,
        Clean,
        ChangeOutfit,
        Status,
        Count
    };

    typedef bool (Game::*CommandCanExecute)() const;
    typedef void (Game::*CommandExecute)();

    struct CommandSpec
    {
        const char *label;
        CommandCanExecute canExecute;
        CommandExecute execute;
    };

    Pet &pet;
    PetStorage &petStorage;
    Renderer &renderer;
    AppearanceLoader &appearanceLoader;
#if ENABLE_GUESS_ITEM_GAME
    GuessItemGame *guessItem;
#endif

    unsigned long last_tick_time = 0;
    std::deque<Animation> animationQueue = {};
    Animation activeAnimation = {};
    bool hasActiveAnimation = false;
    AnimationId baseAnimationId = AnimationId::Idle;
    long displayDuration = 0;
    int lastSelected = 0;
    uint8_t saveCounter = 0;
    long environmentCooldown = 0;
    bool dirtySelect = true;
    bool dirtyAnimation = true;
    bool animateDone = true;
    unsigned long frameInterval = frameIntervalSlow;
    unsigned long lastFrameTime = 0;
    AnimationId showAnimationId = AnimationId::None;
    int selectedSlot = 0;
    char defaultSpeciesCode[9] = "dino";
    char defaultOutfitCode[9] = "base";
    bool selectingOutfit = false;
    char outfitOptions[maxOutfitOptions][9] = {};
    size_t outfitOptionCount = 0;
    size_t selectedOutfitIndex = 0;
    OutfitPreview selectedOutfitPreview = {};
    bool hasSelectedOutfitPreview = false;
    uint16_t outfitPreviewFrame = 1;
    unsigned long outfitPreviewInterval = frameIntervalSlow;
    unsigned long lastOutfitPreviewFrameTime = 0;
    bool dirtyOutfitPreview = false;

    static const CommandSpec commandRegistry[];
    static const Command profileCommands[];
    static const char *commandLabel(Command command);
    static int commandCount();
    static const CommandSpec *commandSpec(Command command);
    static Command commandForSlot(int slot);
    static AnimationId fortuneToAnimationId(int fortuneIndex);

    void NextCommand();
    void PrevCommand();
    void ExecuteCommand();
    void ControlAnimation(unsigned long elapsed);
    void RenderGame(unsigned long now);
    void draw_all_layout();
    void draw_select_layout();
    void roll_sick();
    void roll_fortune();
    AnimationId currentBaseAnimation() const;
    void refreshBaseAnimation();
    void maybeSavePet();
    void maybeDecayEnvironment(unsigned long elapsed);
    void tryStartNextAnimation();
    void queuePostCommandHappyAnimation();
    void queueGiftAnimation();
    bool canPlayGuessItemGame() const;
    bool applyAppearance(const char *speciesCode, const char *outfitCode);
    bool applySpeciesForHealthyDays();
    bool isCommandEnabled(Command command);
    bool canAlwaysExecute() const;
    bool canExecuteNever() const;
    bool canFeedPet() const;
    bool canPredict() const;
    bool canMedicine() const;
    bool canShower() const;
    bool canClean() const;
    bool canChangeOutfit() const;
    bool canStatus() const;
    void queueStatusAnimation();
    bool queueStatusDirectAnimation();
    bool queueStatusAgeAnimation();
    bool queueStatusRandom3Animation();
    bool loadOutfitOptions();
    bool loadSelectedOutfitPreview();
    void changeOutfitSelection(int delta);
    void renderOutfitPreview(unsigned long now);
    void confirmOutfitSelection();
    void exitOutfitSelection();
    void executeNoOp();
    void executeFeedPet();
    void executePredict();
    void executeGift();
    void executeMedicine();
    void executeShower();
    void executeHaveFun();
    void executeClean();
    void executeChangeOutfit();
    void executeStatus();
    bool hasAnimations(const AnimationId *ids, size_t count) const;
};

#endif // GAME_H
