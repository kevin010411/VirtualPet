
#ifndef GUESSAPPLEGAME_H
#define GUESSAPPLEGAME_H

#include "Renderer.h"
#include "Animation.h"
#include "Pet.h"
#include <queue>

enum class State
{
    Inactive,
    WaitingApple,
    WaitingInput,
    ShowingResult,
    Win,
    Lose,
    Cancel
};
enum class Side
{
    Left,
    Right
};

class GuessAppleGame
{
public:
    GuessAppleGame(Pet *pet, Renderer *renderer, std::queue<Animation> *animQueue, bool *calloutAnimate);

    void start(); // 開始玩這個遊戲
    void reset();
    void update(unsigned long dt); // 每次 gameTick 呼叫
    void onLeft();                 // 玩家按左
    void onRight();                // 玩家按右
    void onMid();                  // 玩家按中
    bool isActive() const;         // 現在有沒有在玩
    bool isFinished() const;       // 玩完了嗎
    bool isWin() const;            // 有沒有贏

private:
    State state;
    int correctCount;
    int wrongCount;
    Side appleSide;
    Renderer *renderer;
    Pet *pet;
    bool *dirtyAnimate;
    std::queue<Animation> *animationQueue; // 看你要不要把 queue 傳進來用，或改成 callback
    unsigned long lastMoveTime = 0;
    void handleGuess(Side player);
};

#endif
