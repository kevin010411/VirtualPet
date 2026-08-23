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
    virtual void queueAnimation(const Animation &animation) = 0;
    virtual bool queueCompleteAnimation(AnimationId id,
                                        AnimationOwner owner,
                                        AnimationPriority priority) = 0;
    virtual void clearAnimationsByOwner(AnimationOwner owner) = 0;
    virtual void markAnimationDirty() = 0;
    virtual bool hasAnimation(AnimationId id) const = 0;
    virtual bool hasAnimationPending(AnimationId id) const = 0;
    virtual bool hasAnimationForOwner(AnimationOwner owner) const = 0;
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
    bool isActive() const;

private:
    bool hasItemPromptAnimations() const;
    void queuePromptAnimation(bool replaceExisting = true);
    void handleGuess(GuessItemSide player);

    GuessItemGameHost &host;
    GuessItemState state;
    int correctCount;
    int wrongCount;
    GuessItemSide itemSide;
    AnimationId promptAnimationId;
    unsigned long lastMoveTime = 0;
};

#endif // ENABLE_GUESS_GAME

#endif
