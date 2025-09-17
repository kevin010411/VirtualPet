#include <Adafruit_ST7735.h>
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

struct Animation
{
    String name;
    int duration;
    Animation(String animation_name, int D) : name(animation_name), duration(D) {}
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
        lastSelected = static_cast<int>(nowCommand);
        draw_all_layout();
        dirty_select = true;        
        dirty_animation = true;     
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
                if (animation_queue.back().name != "idle")
                {
                    animation_queue.pop_back();
                }
                displayDuration = animation_queue.back().duration;
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

            // 動畫節拍：到時間才重畫下一幀
            bool frame_due = (current_time - lastFrameTime >= frameInterval);
            if (frame_due) lastFrameTime = current_time;

            // === 最小重繪：只畫「髒」的部分 ===
            if (dirty_animation || frame_due)
            {
                renderer.DisplayAnimation(animation_queue.back().name);
                dirty_animation = false;
            }

            if (dirty_select)
            {
                draw_select_layout(); // 你原本就只畫 32x32 的選取格，保留這個最小重繪
                dirty_select = false;
            }
        }
    }

    String NowCommand()
    {
        return COMMANDS[nowCommand];
    }
    void NextCommand()
    {
        lastSelected = static_cast<int>(nowCommand);
        nowCommand = static_cast<CommandTable>((nowCommand + 1) % NUM_COMMANDS);
        dirty_select = true;
    }
    void PrevCommand()
    {
        lastSelected = static_cast<int>(nowCommand);
        if (nowCommand - 1 < 0)
            nowCommand = static_cast<CommandTable>(nowCommand - 1 + NUM_COMMANDS);
        else
            nowCommand = static_cast<CommandTable>(nowCommand - 1);
        dirty_select = true;
    }
    void ExecuteCommand()
    {
        parse_command(nowCommand);
        dirty_select = true;
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
        }
    }

private:
    const unsigned long gameTick = 1000;
    Pet pet;
    Renderer renderer;
    Adafruit_ST7735 *tft;
    unsigned long last_tick_time;
    std::vector<Animation> animation_queue = {Animation("idle", 1000000)};
    int displayDuration = 0;
    bool isPredictTime = false;
    int lastSelected = 0; 
    int environment_value;
    int environment_cooldown;

    // --- Dirty flags ---
    bool dirty_select = true;      // 選取框 / 指令高亮
    bool dirty_animation = true;   // 主動畫

    // 動畫間隔
    const unsigned long frameInterval = 100;  // 每幀 100ms，可自行調整
    unsigned long lastFrameTime = 0;

    enum CommandTable
    {
        FEED_PET,HAVE_FUN,UNKNOWN,MEDICINE,CLEAN,PREDICT,GIFT,READ,
    };

    const String COMMANDS[8] = {
        "FEED_PET","HAVE_FUN","UNKNOWN","MEDICINE","CLEAN","PREDICT","GIFT","READ",
    };
    const int NUM_COMMANDS = sizeof(COMMANDS) / sizeof(COMMANDS[0]);
    CommandTable nowCommand = FEED_PET;

    void parse_command(CommandTable command)
    {
        switch (command)
        {
        case FEED_PET:
            pet.feedPet(10);
            animation_queue.push_back(Animation("happy", gameTick * 3));
            animation_queue.push_back(Animation("feed", gameTick * 7));
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
            animation_queue.push_back(Animation("happy", gameTick * 3));
            animation_queue.push_back(Animation("clean", gameTick * 5));
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
        if (!animation_queue.empty())
            displayDuration = animation_queue.back().duration;
        else
            displayDuration = 0;
    }

    void draw_select_layout()
    {
        // 還原上一個選擇
        int prevIdx = lastSelected;
        int y_prev = (prevIdx >= 4) ? 160 - 32 : 0;
        char path_prev[24];
        sprintf(path_prev, "/layout/%d.bmp", prevIdx + 1);
        renderer.ShowSDCardImage(path_prev, (prevIdx % 4) * 32, y_prev);
        
        //改變現在選擇的內容
        int curIdx  = static_cast<int>(nowCommand);
        int y_cur = (curIdx >= 4) ? 160 - 32 : 0;

        char path_sel[28];
        sprintf(path_sel, "/layout_sel/%d.bmp", curIdx + 1);
        renderer.ShowSDCardImage(path_sel, (curIdx % 4) * 32, y_cur);
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
