#ifndef GAME_H
#define GAME_H

#include <Arduino.h>
#include <deque>
#include <SdFat.h>
#include "Animation.h"
#include "GuessAppleGame.h"

class Adafruit_ST7735;
class Pet;
class PetStorage;
class Renderer;

class Game : public GuessAppleGameHost
{
public:
    Game(Adafruit_ST7735 *ref_tft, SdFat *ref_SD);
    ~Game();

    void setup_game();
    void loop_game();
    void requestFullRedraw();

    String NowCommand();
    void OnLeftKey();
    void OnRightKey();
    void OnConfirmKey();
    void resetPet();

    void queueAnimation(const Animation &animation) override;
    void clearAnimationsByOwner(AnimationOwner owner) override;
    void markAnimationDirty() override;
    void changePetMood(int delta) override;

private:
    static constexpr unsigned long gameTick = 1200;
    static constexpr unsigned long frameIntervalSlow = 600;
    static constexpr unsigned long frameIntervalFast = 150;
    static constexpr uint8_t savePeriodTicks = 2;
    static constexpr unsigned int environmentDecayAmount = 5;
    static constexpr int maxFortune = 11;

    enum class Command
    {
        FeedPet,
        HaveFun,
        Clean,
        Medicine,
        Shower,
        Predict,
        Gift,
        Status,
        Count
    };

    Pet *pet;
    PetStorage *petStorage;
    Renderer *renderer;
    GuessAppleGame *guessApple;
    Adafruit_ST7735 *tft;
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
};

#endif // GAME_H
