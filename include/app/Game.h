#ifndef GAME_H
#define GAME_H

#include <Arduino.h>
#include <deque>
#include <SdFat.h>
#include "domain/Animation.h"
#include "minigame/GuessItemGame.h"
#include "render/Renderer.h"

class Adafruit_ST7735;
class Pet;
class PetStorage;

class Game : public GuessItemGameHost
{
public:
    Game(Adafruit_ST7735 *ref_tft, SdFat *ref_SD);
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

    void queueAnimation(const Animation &animation) override;
    void clearAnimationsByOwner(AnimationOwner owner) override;
    void markAnimationDirty() override;
    void changePetMood(int delta) override;
    bool hasAnimation(AnimationId id) const override;
    bool hasAnimationForOwner(AnimationOwner owner) const override;

private:
    static constexpr unsigned long gameTick = 1200;
    static constexpr unsigned long frameIntervalSlow = 600;
    static constexpr uint8_t savePeriodTicks = 2;
    static constexpr unsigned int environmentDecayAmount = 5;
    static constexpr int maxFortune = 11;

    enum class Command
    {
        FeedPet,
        Predict,
        Gift,
        Medicine,
        Shower,
        HaveFun,
        Clean,
        Status,
        Count
    };

    Pet *pet;
    PetStorage *petStorage;
    Renderer *renderer;
    GuessItemGame *guessItem;
    SdFat *sd;

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
    Command nowCommand = Command::FeedPet;
    char defaultSpeciesCode[9] = "dino";
    char defaultOutfitCode[9] = "base";

    static const char *commandLabel(Command command);
    static int commandCount();
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
    void parse_command(Command command);
    void refreshBaseAnimation();
    void maybeSavePet();
    void maybeDecayEnvironment(unsigned long elapsed);
    void tryStartNextAnimation();
    void queuePostCommandHappyAnimation();
    bool applyAppearance(const char *speciesCode, const char *outfitCode);
    bool applySpeciesForHealthyDays();
    bool isCommandEnabled(Command command);
    bool hasAnimations(const AnimationId *ids, size_t count) const;
};

#endif // GAME_H
