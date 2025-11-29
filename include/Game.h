#ifndef GAME_H
#define GAME_H

#include "Renderer.h"
#include <vector>
#include <queue>

template <typename T>
static inline T clamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

struct PetConfig
{
    float max_age = 100;               // 歲
    uint8_t max_hunger = 100;          // 0=不餓, 100=超餓
    uint8_t max_mood = 100;            // 0=極差, 100=極好
    unsigned int max_clean = 300;      // 0=極差, 300=極好
    unsigned int max_env_clean = 1000; // 0=極差, 1000=極好
    float age_per_tick = 0.2f;         // 每 tick 增齡
    uint16_t min_stay_ticks = 10;      // 任一狀態最短停留（避免抖動）
};

enum class HealthStatus;
HealthStatus decide_state(uint8_t hunger, uint8_t mood, unsigned int env_value,
                          unsigned int clean_value, bool hasSick, const PetConfig &cfg);

class Pet
{
public:
    Pet(Adafruit_ST7735 *ref_tft, float age = 0);
    void dayPassed();
    void feedPet(int add_satiety);
    void changeMood(int delta);
    void takeShower(int value);
    void cleanEnv(unsigned int clear_value);
    void getSick();
    bool checkHealth();
    HealthStatus getStatus() const;
    String CurrentAnimation() const;

private:
    PetConfig cfg;
    bool hasSick;
    float age;
    int hungry_value;
    int mood;
    int clean_value;
    int env_value;
    Adafruit_ST7735 *tft;
    HealthStatus status;
};

struct Animation
{
    String name;
    int duration;
    Animation(String n, int d);
};

class Game
{
public:
    Game(Adafruit_ST7735 *ref_tft, SdFat *ref_SD);
    void setup_game();
    void loop_game();

    String NowCommand();
    void NextCommand();
    void PrevCommand();
    void ExecuteCommand();

private:
    const unsigned long gameTick = 1000;
    Pet pet;
    Renderer renderer;
    Adafruit_ST7735 *tft;
    unsigned long last_tick_time;
    std::queue<Animation> animationQueue = {};
    String baseAnimationName;
    long displayDuration = 0;
    bool isPredictTime = false;
    int lastSelected = 0;
    int environment_value;
    int environment_cooldown;

    // --- Dirty flags ---
    bool dirty_select = true;   // 選取框 / 指令高亮
    bool dirtyAnimation = true; // 主動畫

    enum CommandTable
    {
        FEED_PET,
        HAVE_FUN,
        CLEAN,
        MEDICINE,
        SHOWER,
        PREDICT,
        GIFT,
        READ,
    };

    const String COMMANDS[8] = {
        "FEED_PET",
        "HAVE_FUN",
        "CLEAN",
        "MEDICINE",
        "SHOWER",
        "PREDICT",
        "GIFT",
        "READ",
    };

    const int NUM_COMMANDS = sizeof(COMMANDS) / sizeof(COMMANDS[0]);
    CommandTable nowCommand = FEED_PET;

    // 動畫間隔
    const unsigned long frameInterval = 100; // 每幀 100ms，可自行調整
    unsigned long lastFrameTime = 0;
    void control_pet_animation(unsigned long dt);
    void render_game(unsigned long now);
    void draw_all_layout();
    void draw_select_layout();
    void roll_sick();
    void parse_command(CommandTable command);
};

#endif // GAME_H