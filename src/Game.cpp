#include "Game.h"
#include <Arduino.h>

enum class HealthStatus
{
    Healthy,
    Hungry,
    Depressed,
    Dirty,
    Poop,
    Sick,
};

HealthStatus decide_state(
    uint8_t hunger, uint8_t mood, unsigned int env_value, unsigned int clean_value, bool hasSick, const PetConfig &cfg)
{
    // 優先權：Sick > Hungry > Depressed > Poop >  Dirty > Healthy
    if (hasSick)
        return HealthStatus::Sick;
    if (hunger >= 70)
        return HealthStatus::Hungry;
    if (mood <= 30)
        return HealthStatus::Depressed;
    if (env_value <= 300)
        return HealthStatus::Poop;
    if (clean_value <= 100)
        return HealthStatus::Dirty;
    return HealthStatus::Healthy;
}

Pet::Pet(Adafruit_ST7735 *ref_tft, float age) : age(age), hungry_value(50), mood(50),
                                                env_value(1000), clean_value(300), hasSick(false),
                                                tft(ref_tft), status(HealthStatus::Healthy) {}

void Pet::dayPassed()
{
    hungry_value = clamp<int>(hungry_value + 1, 0, cfg.max_hunger);
    mood = clamp<int>(mood - 1, 0, cfg.max_mood);
    age = clamp<float>(age + cfg.age_per_tick, 0.0f, cfg.max_age);
    clean_value = clamp<int>(clean_value - 1, 0, cfg.max_clean);
    env_value = clamp<int>(env_value - 1, 0, cfg.max_env_clean);

    status = decide_state(hungry_value, mood, env_value, clean_value, hasSick, cfg);
}

void Pet::feedPet(int add_satiety)
{
    hungry_value = clamp<int>(hungry_value - add_satiety, 0, cfg.max_hunger);
}

void Pet::changeMood(int delta)
{
    mood = clamp<int>(mood + delta, 0, cfg.max_mood);
}

void Pet::takeShower(int value)
{
    clean_value = clamp<int>(clean_value + value, 0, cfg.max_clean);
}

void Pet::cleanEnv(unsigned int clear_value)
{
    env_value = clamp<int>(env_value + clear_value, 0, cfg.max_env_clean);
}

void Pet::getSick()
{
    hasSick = true;
}

bool Pet::checkHealth()
{
    if (!hasSick)
        return false;
    hasSick = false;
    return true;
}

String Pet::CurrentAnimation() const
{
    switch (status)
    {
    case HealthStatus::Healthy:
        return "idle"; // 平時
    case HealthStatus::Hungry:
        return "hungry"; // 肚子餓
    case HealthStatus::Depressed:
        return "depress"; // 心情差
    case HealthStatus::Sick:
        return "sick"; // 生病
    case HealthStatus::Dirty:
        return "dirty"; // 臭臭
    case HealthStatus::Poop:
        return "poop"; // 大便
    }
    return "idle";
}

Animation::Animation(String n, int d)
    : name(n), duration(d) {}

Game::Game(Adafruit_ST7735 *ref_tft, SdFat *ref_SD)
    : pet(Pet(ref_tft)), renderer(Renderer(ref_tft, ref_SD)),
      tft(ref_tft), last_tick_time(0), environment_value(10), environment_cooldown(0),
      nowCommand(FEED_PET)
{
}

void Game::setup_game()
{
    renderer.initAnimations();
    lastSelected = static_cast<int>(nowCommand);
    draw_all_layout();
    dirty_select = true;
    dirtyAnimation = true;
}

void Game::loop_game()
{
    unsigned long current_time = millis();
    unsigned long time_comsumed = current_time - last_tick_time;
    // 每一個game tick更新一天的狀態
    if (time_comsumed >= gameTick)
    {
        environment_cooldown -= time_comsumed;
        last_tick_time = current_time;
        control_pet_animation(time_comsumed);
        if (environment_cooldown <= 0)
        {
            environment_value = environment_value - 5;
            if (environment_value < 0)
                environment_value = 0;
            environment_cooldown = random(3 * gameTick, 6 * gameTick);
        }
        roll_sick();
        pet.dayPassed();

        baseAnimationName = pet.CurrentAnimation();
    }
    // 時間到或有dirty_animation或dirty select才重畫
    render_game(current_time);
}

void Game::control_pet_animation(unsigned long time_comsumed)
{
    displayDuration -= time_comsumed;
    if (displayDuration > 0)
        return;

    if (!animationQueue.empty())
        animationQueue.pop();

    if (!animationQueue.empty())
        displayDuration = animationQueue.back().duration;
}

void Game::render_game(unsigned long current_time)
{
    bool frame_due = (current_time - lastFrameTime >= frameInterval);
    if (frame_due)
        lastFrameTime = current_time;

    // === 最小重繪：只畫「髒」的部分 ===
    if (dirtyAnimation || frame_due)
    {
        const String &toShow = (!animationQueue.empty())
                                   ? animationQueue.back().name
                                   : baseAnimationName;

        renderer.DisplayAnimation(toShow);
        dirtyAnimation = false;
    }

    if (dirty_select)
    {
        draw_select_layout(); // 你原本就只畫 32x32 的選取格，保留這個最小重繪
        dirty_select = false;
    }
}

String Game::NowCommand()
{
    return COMMANDS[nowCommand];
}
void Game::NextCommand()
{
    lastSelected = static_cast<int>(nowCommand);
    nowCommand = static_cast<CommandTable>((nowCommand + 1) % NUM_COMMANDS);
    dirty_select = true;
}
void Game::PrevCommand()
{
    lastSelected = static_cast<int>(nowCommand);
    if (nowCommand - 1 < 0)
        nowCommand = static_cast<CommandTable>(nowCommand - 1 + NUM_COMMANDS);
    else
        nowCommand = static_cast<CommandTable>(nowCommand - 1);
    dirty_select = true;
}
void Game::ExecuteCommand()
{
    parse_command(nowCommand);
    dirty_select = true;
}
void Game::draw_all_layout()
{
    for (int i = 0; i < 8; ++i)
    {
        int y_start = 0;
        if (i >= 4)
            y_start = 160 - 32;
        char path[20]; // 緩衝區大小視需求調整
        sprintf(path, "/layout/%d.bmp", i + 1);
        renderer.ShowSDCardImage(path, i % 4 * 32, y_start);
    }
}

void Game::parse_command(CommandTable command)
{
    switch (command)
    {
    case FEED_PET:
        pet.feedPet(40);
        animationQueue.push(Animation("feed", gameTick * 7));
        animationQueue.push(Animation("happy", gameTick * 3));
        break;
    case HAVE_FUN:
        pet.changeMood(30);
        animationQueue.push(Animation("happy", gameTick * 3));
        break;
    case CLEAN:
        pet.cleanEnv(300);
        animationQueue.push(Animation("clean", gameTick * 5));
        animationQueue.push(Animation("happy", gameTick * 3));
        break;
    case MEDICINE:
        pet.checkHealth();
        animationQueue.push(Animation("heal", gameTick * 5));
        animationQueue.push(Animation("happy", gameTick * 3));
        break;
    case SHOWER:
        pet.takeShower(250);
        animationQueue.push(Animation("shower", gameTick * 5));
        animationQueue.push(Animation("happy", gameTick * 3));
        break;
    case PREDICT:
        break;
    case GIFT:
        break;
    case READ:
        break;
    default:
        Serial.println(command);
        Serial.println(F("指令錯誤"));
        break;
    }
    if (!animationQueue.empty())
        displayDuration = animationQueue.back().duration;
    else
        displayDuration = 0;
}

void Game::draw_select_layout()
{
    // 還原上一個選擇
    int prevIdx = lastSelected;
    int y_prev = (prevIdx >= 4) ? 160 - 32 : 0;
    char path_prev[24];
    sprintf(path_prev, "/layout/%d.bmp", prevIdx + 1);
    renderer.ShowSDCardImage(path_prev, (prevIdx % 4) * 32, y_prev);

    // 改變現在選擇的內容
    int curIdx = static_cast<int>(nowCommand);
    int y_cur = (curIdx >= 4) ? 160 - 32 : 0;

    char path_sel[28];
    sprintf(path_sel, "/layout_sel/%d.bmp", curIdx + 1);
    renderer.ShowSDCardImage(path_sel, (curIdx % 4) * 32, y_cur);
}

void Game::roll_sick()
{
    float probability = 0.01; // 機率函數 P(x)

    // 生成 0 ~ 1 的隨機數
    float randValue = random(1001) / 1000.0; // 隨機數範圍 [0, 1]

    if (randValue < probability)
    {
        pet.getSick();
    }
}
