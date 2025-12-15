#ifndef GAME_H
#define GAME_H

#include "Renderer.h"
#include "Animation.h"
#include "GuessAppleGame.h"
#include "Pet.h"
#include <vector>
#include <queue>

class Game
{
public:
    Game(Adafruit_ST7735 *ref_tft, SdFat *ref_SD);
    void setup_game();
    void loop_game();

    String NowCommand();
    void OnLeftKey();
    void OnRightKey();
    void OnConfirmKey();

private:
    const unsigned long gameTick = 1200;
    const short savePeriod = 1;
    Pet pet;
    Renderer renderer;
    Adafruit_ST7735 *tft;
    unsigned long last_tick_time;
    std::queue<Animation> animationQueue = {};
    String baseAnimationName;
    long displayDuration = 0;
    bool isPredictTime = false;
    int lastSelected = 0;
    short savePatient = 0;
    int environment_value;
    int environment_cooldown;
    GuessAppleGame guessApple;
    // --- Dirty flags ---
    bool dirtySelect = true;    // 選取框 / 指令高亮
    bool dirtyAnimation = true; // 主動畫
    bool animateDone = true;

    enum CommandTable
    {
        FEED_PET,
        HAVE_FUN,
        CLEAN,
        MEDICINE,
        SHOWER,
        PREDICT,
        GIFT,
        STATUS,
    };

    const String COMMANDS[8] = {
        "FEED_PET",
        "HAVE_FUN",
        "CLEAN",
        "MEDICINE",
        "SHOWER",
        "PREDICT",
        "GIFT",
        "STATUS",
    };

    const int NUM_COMMANDS = sizeof(COMMANDS) / sizeof(COMMANDS[0]);
    CommandTable nowCommand = FEED_PET;

    // 動畫間隔
    unsigned long frameInterval = 500; // 每幀 100ms，可自行調整
    unsigned long lastFrameTime = 0;
    String ShowAnimate = "";

    void NextCommand();
    void PrevCommand();
    void ExecuteCommand();
    void ControlAnimation(unsigned long dt);
    void RenderGame(unsigned long now);
    void draw_all_layout();
    void draw_select_layout();
    void roll_sick();
    void parse_command(CommandTable command);

    const int MAX_FORTUNE = 11;
    void roll_fortune();
};

#endif // GAME_H