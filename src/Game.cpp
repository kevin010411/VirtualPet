#include "Game.h"
#include <Arduino.h>

Game::Game(Adafruit_ST7735 *ref_tft, SdFat *ref_SD)
    : pet(Pet(ref_tft)), renderer(Renderer(ref_tft, ref_SD)),
      tft(ref_tft), last_tick_time(0), environment_value(10), environment_cooldown(0),
      nowCommand(FEED_PET), guessApple(&pet, &renderer, &animationQueue, &dirtyAnimation)
{
}

void Game::setup_game()
{
    renderer.initAnimations();
    lastSelected = static_cast<int>(nowCommand);
    draw_all_layout();
    dirtySelect = true;
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
        ControlAnimation(time_comsumed);
        if (environment_cooldown <= 0)
        {
            environment_value = environment_value - 5;
            if (environment_value < 0)
                environment_value = 0;
            environment_cooldown = random(3 * gameTick, 6 * gameTick);
        }
        if (animationQueue.empty())
        {
            roll_sick();
            pet.dayPassed();
        }
        guessApple.update(time_comsumed);

        String cadidateAnimate = pet.CurrentAnimation();
        if (baseAnimationName != cadidateAnimate)
        {
            baseAnimationName = cadidateAnimate;
            dirtyAnimation = true;
        }
    }
    // 時間到或有dirty_animation或dirty select才重畫
    RenderGame(current_time);
}

void Game::ControlAnimation(unsigned long time_comsumed)
{
    if (animationQueue.empty())
        return;

    displayDuration -= time_comsumed;
    if (displayDuration > 0 && !animateDone)
        return;

    animationQueue.pop();
    if (!animationQueue.empty())
        displayDuration = animationQueue.front().duration;
    else
        displayDuration = 0;
    dirtyAnimation = true;
}

void Game::RenderGame(unsigned long current_time)
{
    bool frame_due = (current_time - lastFrameTime >= frameInterval);
    if (frame_due)
        lastFrameTime = current_time;

    // === 最小重繪：只畫「髒」的部分 ===
    if (dirtyAnimation || ShowAnimate == "")
    {
        bool showOnce;
        if (!animationQueue.empty())
        {
            ShowAnimate = animationQueue.front().name;
            showOnce = animationQueue.front().showOnce;
        }
        else
        {
            ShowAnimate = baseAnimationName;
            showOnce = false; // 平常 idle 不需要 showOneTime
        }

        renderer.DisplayAnimation(ShowAnimate, showOnce);
        dirtyAnimation = false;
    }
    else if (frame_due)
    {
        animateDone |= renderer.DisplayAnimation();
    }

    if (dirtySelect)
    {
        draw_select_layout(); // 你原本就只畫 32x32 的選取格，保留這個最小重繪
        dirtySelect = false;
    }
}

void Game::OnLeftKey()
{
    if (guessApple.isActive())
        guessApple.onLeft();
    else
        NextCommand();
}

void Game::OnRightKey()
{
    if (guessApple.isActive())
        guessApple.onRight();
    else
        PrevCommand();
}

void Game::OnConfirmKey()
{
    if (guessApple.isActive()) // 小遊戲中 confirm 可以忽略，或做「跳過」之類的功能
        return;
    ExecuteCommand();
}

String Game::NowCommand()
{
    return COMMANDS[nowCommand];
}
void Game::NextCommand()
{
    lastSelected = static_cast<int>(nowCommand);
    nowCommand = static_cast<CommandTable>((nowCommand + 1) % NUM_COMMANDS);
    dirtySelect = true;
}
void Game::PrevCommand()
{
    lastSelected = static_cast<int>(nowCommand);
    if (nowCommand - 1 < 0)
        nowCommand = static_cast<CommandTable>(nowCommand - 1 + NUM_COMMANDS);
    else
        nowCommand = static_cast<CommandTable>(nowCommand - 1);
    dirtySelect = true;
}
void Game::ExecuteCommand()
{
    parse_command(nowCommand);
}

void Game::parse_command(CommandTable command)
{
    switch (command)
    {
    case FEED_PET:
        pet.feedPet(40);
        animationQueue.push(Animation("feed", gameTick * 3.5));
        animationQueue.push(Animation("happy", gameTick * 1.5));
        dirtyAnimation = true;
        break;
    case HAVE_FUN:
        guessApple.start();
        break;
    case CLEAN:
        pet.cleanEnv(300);
        animationQueue.push(Animation("clean", gameTick * 3.5));
        animationQueue.push(Animation("happy", gameTick * 1.5));
        dirtyAnimation = true;
        break;
    case MEDICINE:
        pet.takeMedicine();
        animationQueue.push(Animation("heal", gameTick * 3.5));
        animationQueue.push(Animation("happy", gameTick * 1.5));
        dirtyAnimation = true;
        break;
    case SHOWER:
        pet.takeShower(250);
        animationQueue.push(Animation("shower", gameTick * 3.5));
        animationQueue.push(Animation("happy", gameTick * 1.5));
        dirtyAnimation = true;
        break;
    case PREDICT:
        animationQueue.push(Animation("pred_anim", gameTick * 20, true));
        roll_fortune();
        dirtyAnimation = true;
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
    {
        displayDuration = animationQueue.front().duration;
        animateDone = false;
    }
    else
        displayDuration = 0;
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

void Game::roll_fortune()
{
    String fortune_idx = String(random(1, MAX_FORTUNE + 1));
    animationQueue.push(Animation("predict_" + fortune_idx, gameTick * 5));
    dirtyAnimation = true;
}
