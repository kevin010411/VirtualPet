#ifndef GUESSITEMGAME_H
#define GUESSITEMGAME_H

#include "shared/config/AppProfile.h"

#if ENABLE_GUESS_GAME

#include <Arduino.h>
#include "animation/domain/Animation.h"

enum class GuessItemState
{
    Inactive,
    Starting,
    WaitingItem,
    WaitingInput,
    ShowingResult,
    Win,
    Lose,
    Cancel
};

enum class GuessItemSide
{
    Left,
    Right
};

enum class GuessItemOutcome
{
    RoundCorrect,
    RoundWrong,
    GameWin,
    GameLoss,
};

class GuessItemGameHost
{
public:
    virtual ~GuessItemGameHost() = default;
    virtual PlaybackResult replace(const AnimationSequence &sequence) = 0;
    virtual void cancelPlayback() = 0;
    virtual bool isPlaybackBusy() const = 0;
    virtual bool hasAnimation(FirmwarePlaybackRole id) const = 0;
    virtual bool hasAnimationPending(FirmwarePlaybackRole id) const = 0;
    virtual void settleOutcome(GuessItemOutcome outcome) = 0;
};

class GuessItemGame
{
public:
    explicit GuessItemGame(GuessItemGameHost &host);

    void start();
    void reset();
    void update();
    void onLeft();
    void onRight();
    void onMid();
    void onPlaybackFailed();
    bool isActive() const;

private:
    bool hasItemPromptAnimations() const;
    PlaybackResult replacePromptAnimation();
    void handleGuess(GuessItemSide player);

    GuessItemGameHost &host;
    GuessItemState state;
    int correctCount;
    int wrongCount;
    GuessItemSide itemSide;
    FirmwarePlaybackRole promptPlaybackRole;
    unsigned long lastMoveTime = 0;
};

#endif // ENABLE_GUESS_GAME

#endif
