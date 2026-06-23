#ifndef GAME_H
#define GAME_H

#include <Arduino.h>
#include <deque>
#include "app/CommandController.h"
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

class Game : public CommandHost
#if ENABLE_GUESS_ITEM_GAME
    ,
             public GuessItemGameHost
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
    bool startStartupAnimation();
    void startBatteryAnimation();
    void updateBatteryAnimation(unsigned long now);

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

    Pet &pet;
    PetStorage &petStorage;
    Renderer &renderer;
    AppearanceLoader &appearanceLoader;
    CommandController commands;
#if ENABLE_GUESS_ITEM_GAME
    GuessItemGame *guessItem;
#endif

    unsigned long last_tick_time = 0;
    std::deque<Animation> animationQueue = {};
    Animation activeAnimation = {};
    bool hasActiveAnimation = false;
    AnimationId baseAnimationId = AnimationId::Idle;
    long displayDuration = 0;
    uint8_t saveCounter = 0;
    long environmentCooldown = 0;
    bool dirtySelect = true;
    bool dirtyAnimation = true;
    bool animateDone = true;
    unsigned long frameInterval = frameIntervalSlow;
    unsigned long lastFrameTime = 0;
    AnimationId showAnimationId = AnimationId::None;
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

    static AnimationId fortuneToAnimationId(int fortuneIndex);

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
    bool hasAnimations(const AnimationId *ids, size_t count) const;

    bool commandHasAnimation(AnimationId id) const override;
    AnimationId commandCurrentAgeAnimation() const override;
    void commandClearCommandAnimations() override;
    void commandFeedPet() override;
    void commandPredict() override;
    void commandGift() override;
    void commandMedicine() override;
    void commandShower() override;
#if ENABLE_GUESS_ITEM_GAME
    void commandHaveFun() override;
#endif
    void commandClean() override;
    void commandChangeOutfit() override;
    void commandStatus() override;
};

#endif // GAME_H
