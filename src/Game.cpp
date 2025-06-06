﻿#include <Adafruit_ST7735.h>
#include <SdFat.h>
#include <vector>
#include "Renderer.cpp"

class Pet
{
public:
    Pet(String name, Adafruit_ST7735 *ref_tft, float age = 0, String type = "Chicken")
        : name(name), type(type), age(age), max_age(100), max_hungry(100),
          hungry_value(50), mood(50), max_mood(100), bad_heathly_duration(0),
          tft(ref_tft), status(Healthy) {}

    void dayPassed()
    {
        if (hungry_value <= max_hungry)
            hungry_value += 1;
        if (mood > 0)
            mood -= 1;
        if (age < max_age)
            age += 0.2f;
        if (status != Healthy)
            bad_heathly_duration += 1;

        checkHeathStatus();
    }

    void feedPet(int add_satiety)
    {
        hungry_value += add_satiety;
        if (hungry_value > max_hungry)
            hungry_value = max_hungry;
    }

    void addPressure(int pressure)
    {
        mood += pressure;
        if (mood > max_mood)
            mood = max_mood;
        if (mood < 0)
            mood = 0;
    }

    bool getSick()
    {
        if (status == Death)
            return false;
        status = Sick;
        return true;
    }
    bool medician()
    {
        if (status != Sick)
            return false;
        status = Healthy;
        return true;
    }

private:
    const int death_limit = 200;
    String name;
    String type;
    float age;
    int max_age;
    int max_hungry;
    int hungry_value;
    int mood;
    int max_mood;
    int bad_heathly_duration;
    Adafruit_ST7735 *tft;
    enum HealthStatus
    {
        Healthy,
        Hungry,
        Depressed,
        Sick,
        Death,
    };
    String HealthStatusStr[5] = {
        "Healthy",
        "Hungry",
        "Depressed",
        "Sick",
        "Death",
    };
    HealthStatus status;
    void checkHeathStatus()
    {
        if (bad_heathly_duration >= death_limit)
            status = Death;
        else if (status == Healthy)
        {
            if (mood <= 0)
                status = Depressed;
            else if (hungry_value >= max_hungry)
                status = Hungry;
        }
        else if (mood > 0 && hungry_value < max_hungry)
        {
            status = Healthy;
            bad_heathly_duration = 0;
        }
    }
};

class Game
{
public:
    Game(Adafruit_ST7735 *ref_tft, SdFat *ref_SD, String name = "Mumei")
        : pet(Pet(name, ref_tft)), renderer(Renderer(ref_tft, ref_SD)),
          tft(ref_tft), last_tick_time(0), environment_value(10), environment_cooldown(0),
          nowCommand(FEED_PET)
    {
    }

    void setup_game()
    {
        renderer.initAnimations();
        draw_all_layout();
        Serial.println("Welcome to the Virtual Pet Game!");
    }

    void loop_game()
    {
        unsigned long current_time = millis();
        // 每秒更新一次
        if (current_time - last_tick_time >= gameTick)
        {
            environment_cooldown -= current_time - last_tick_time;
            displayDuration -= current_time - last_tick_time;
            last_tick_time = current_time;
            if (displayDuration <= 0)
            {
                if (animation_queue.back() != "idle")
                {
                    animation_queue.pop_back();
                    animation_duration.pop_back();
                }
                displayDuration = animation_duration.back();
            }
            if (environment_cooldown <= 0)
            {
                environment_value = environment_value - 5;
                if (environment_value < 0)
                    environment_value = 0;
                environment_cooldown = random(3 * gameTick, 6 * gameTick);
            }
            roll_sick();
            pet.dayPassed();
            draw_scene();
        }
    }

    String NowCommand()
    {
        return COMMANDS[nowCommand];
    }
    void NextCommand()
    {
        nowCommand = static_cast<CommandTable>((nowCommand + 1) % NUM_COMMANDS);
    }
    void PrevCommand()
    {
        if (nowCommand - 1 < 0)
            nowCommand = static_cast<CommandTable>(nowCommand - 1 + NUM_COMMANDS);
        else
            nowCommand = static_cast<CommandTable>(nowCommand - 1);
    }
    void ExecuteCommand()
    {
        parse_command(nowCommand);
    }
    void draw_all_layout()
    {
        for (int i = 0; i < 8; ++i)
        {
            int y_start = 0;
            if (i >= 4)
                y_start = 160 - 32;
            char path[20]; // 緩衝區大小視需求調整
            sprintf(path, "/layout/%d.bmp", i + 1);
            renderer.ShowSDCardImage(path, i % 4 * 32, y_start);
            // Serial.println(path);
        }
    }

private:
    const unsigned long gameTick = 1000;
    Pet pet;
    Renderer renderer;
    Adafruit_ST7735 *tft;
    unsigned long last_tick_time;
    std::vector<String> animation_queue = {"idle"};
    std::vector<int> animation_duration = {1000000};
    int displayDuration = 0;
    bool isSelectButtonOn = true;
    bool isPredictTime = false;
    int environment_value;
    int environment_cooldown;

    enum CommandTable
    {
        FEED_PET,
        HAVE_FUN,
        UNKNOWN,
        MEDICINE,
        CLEAN,
        PREDICT,
        GIFT,
        READ,
    };

    const String COMMANDS[8] = {
        "FEED_PET",
        "HAVE_FUN",
        "UNKNOWN",
        "MEDICINE",
        "CLEAN",
        "PREDICT",
        "GIFT",
        "READ",
    };
    const int NUM_COMMANDS = sizeof(COMMANDS) / sizeof(COMMANDS[0]);
    CommandTable nowCommand = FEED_PET;

    void parse_command(CommandTable command)
    {
        switch (command)
        {
        case FEED_PET:
            pet.feedPet(10);
            animation_queue.push_back("happy");
            animation_duration.push_back(gameTick * 3);
            animation_queue.push_back("feed");
            animation_duration.push_back(gameTick * 7);
            displayDuration = gameTick * 5;
            Serial.println(F("Fed pet! Satiety increased."));
            break;
        case HAVE_FUN:
            pet.addPressure(10);
            Serial.println(F("Pet had fun! Mood increased."));
            break;
        case UNKNOWN:
            Serial.println(F("Pet Do Unknown"));
            break;
        case MEDICINE:
            pet.medician();
            Serial.println(F("Pet Do MEDICINE"));
            break;
        case CLEAN:
            environment_value = 10;
            animation_queue.push_back("happy");
            animation_duration.push_back(gameTick * 3);
            animation_queue.push_back("clean");
            animation_duration.push_back(gameTick * 5);
            displayDuration = gameTick * 5;
            Serial.println(F("Pet Do SHOWER"));
            break;
        case PREDICT:
            Serial.println(F("Pet Do PREDICT"));
            break;
        case GIFT:
            Serial.println(F("Pet Do GIFT"));
            break;
        case READ:
            Serial.println(F("Pet Do READ"));
            break;
        default:
            Serial.println(command);
            Serial.println(F("指令錯誤"));
            break;
        }
    }

    void draw_scene()
    {
        if (isPredictTime)
        {
            Serial.println("Good Predict😀");
        }
        renderer.DisplayAnimation(animation_queue.back());
        draw_background();
        draw_select_layout();
    }

    void draw_select_layout()
    {
        int y_start = 0;
        if (nowCommand >= 4)
            y_start = 160 - 32;
        char path[20]; // 緩衝區大小視需求調整
        sprintf(path, "/layout/%d.bmp", nowCommand + 1);
        if (!isSelectButtonOn)
            tft->fillRect(nowCommand % 4 * 32, y_start, 32, 32, tft->color565(255, 255, 255));
        else
            renderer.ShowSDCardImage(path, nowCommand % 4 * 32, y_start);
        // Serial.println(path);
        isSelectButtonOn = !isSelectButtonOn;
    }

    void draw_background()
    {
        if (environment_value < 5)
            draw_poop();
    }

    int poop_index = 1;
    void draw_poop()
    {
        int x_start = 10;
        int y_start = 94;
        char path[20]; // 緩衝區大小視需求調整
        sprintf(path, "/poop/%d.bmp", poop_index);
        poop_index = 3 - poop_index; // 這樣才可以在1,2之間切換
        renderer.ShowSDCardImage(path, x_start, y_start);
    }

    void roll_sick()
    {
        float probability = 1.0 - (float)environment_value / 10.0; // 機率函數 P(x)

        // 生成 0 ~ 1 的隨機數
        float randValue = random(1001) / 1000.0; // 隨機數範圍 [0, 1]

        if (randValue < probability)
        {
            pet.getSick();
        }
    }
};
