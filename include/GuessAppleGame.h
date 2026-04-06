#ifndef GUESSAPPLEGAME_H
#define GUESSAPPLEGAME_H

#include <Arduino.h>
#include "Animation.h"

enum class GuessAppleState
{
    Inactive,
    WaitingApple,
    WaitingInput,
    ShowingResult,
    Win,
    Lose,
    Cancel
};

enum class GuessAppleSide
{
    Left,
    Right
};

class GuessAppleGameHost
{
public:
    virtual ~GuessAppleGameHost() = default;
    virtual void queueAnimation(const Animation &animation) = 0;
    virtual void clearAnimationsByOwner(AnimationOwner owner) = 0;
    virtual void markAnimationDirty() = 0;
    virtual void changePetMood(int delta) = 0;
};

class GuessAppleGame
{
public:
    explicit GuessAppleGame(GuessAppleGameHost &host);

    void start();
    void reset();
    void update();
    void onLeft();
    void onRight();
    void onMid();
    bool isActive() const;
    bool isFinished() const;
    bool isWin() const;

private:
    void queuePromptAnimation();
    void handleGuess(GuessAppleSide player);

    GuessAppleGameHost &host;
    GuessAppleState state;
    int correctCount;
    int wrongCount;
    GuessAppleSide appleSide;
    AnimationId promptAnimationId;
    unsigned long lastMoveTime = 0;
};

#endif
