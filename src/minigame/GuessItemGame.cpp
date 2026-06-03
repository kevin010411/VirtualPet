#include "minigame/GuessItemGame.h"

namespace
{
#ifndef ENABLE_GUESS_GAME_SINGLE_ROUND
#define ENABLE_GUESS_GAME_SINGLE_ROUND 0
#endif

    constexpr unsigned long kItemRevealDelayMs = 50;
    constexpr unsigned long kStartAnimationDurationMs = 1200;
    constexpr unsigned long kItemPromptLoopDurationMs = 24UL * 60UL * 60UL * 1000UL;
    constexpr unsigned long kItemPromptSwitchIntervalMs = 800;
    constexpr unsigned long kResultAnimationDurationMs = 2000;
    constexpr unsigned long kCancelExitDelayMs = 250;
#if ENABLE_GUESS_GAME_SINGLE_ROUND
    constexpr int kMaxGuessCount = 1;
#else
    constexpr int kMaxGuessCount = 3;
#endif

    AnimationId randomItemAnimation()
    {
        switch (random(1, 5))
        {
        case 1:
            return AnimationId::GuessItem1;
        case 2:
            return AnimationId::GuessItem2;
        case 3:
            return AnimationId::GuessItem3;
        default:
            return AnimationId::GuessItem4;
        }
    }

    AnimationId randomItemAnimationExcept(AnimationId current)
    {
        AnimationId next = randomItemAnimation();
        while (next == current)
            next = randomItemAnimation();

        return next;
    }

    AnimationId itemResultAnimation(GuessItemSide itemSide, GuessItemSide playerSide)
    {
        constexpr AnimationId kResultByPetAndItem[2][2] = {
            {AnimationId::GuessLL, AnimationId::GuessLR},
            {AnimationId::GuessRL, AnimationId::GuessRR},
        };

        return kResultByPetAndItem[static_cast<int>(playerSide)][static_cast<int>(itemSide)];
    }
} // namespace

GuessItemGame::GuessItemGame(GuessItemGameHost &hostRef)
    : host(hostRef)
{
    reset();
}

void GuessItemGame::queuePromptAnimation()
{
    host.clearAnimationsByOwner(AnimationOwner::Minigame);
    host.queueAnimation(Animation(promptAnimationId, kItemPromptLoopDurationMs, false, AnimationOwner::Minigame, AnimationPriority::Critical));
    host.markAnimationDirty();
}

void GuessItemGame::start()
{
    reset();
    host.clearAnimationsByOwner(AnimationOwner::Minigame);

    if (host.hasAnimation(AnimationId::GuessStart))
    {
        host.queueAnimation(Animation(AnimationId::GuessStart, kStartAnimationDurationMs, true, AnimationOwner::Minigame, AnimationPriority::Critical));
        host.markAnimationDirty();
        state = GuessItemState::Starting;
    }
    else
    {
        promptAnimationId = randomItemAnimation();
        queuePromptAnimation();
        state = GuessItemState::WaitingItem;
    }

    lastMoveTime = millis();
}

bool GuessItemGame::isActive() const
{
    return state != GuessItemState::Inactive;
}

bool GuessItemGame::isFinished() const
{
    return state == GuessItemState::Win || state == GuessItemState::Lose || state == GuessItemState::Cancel;
}

bool GuessItemGame::isWin() const
{
    return state == GuessItemState::Win;
}

void GuessItemGame::update()
{
    if (state == GuessItemState::Inactive)
        return;

    const unsigned long now = millis();
    switch (state)
    {
    case GuessItemState::Starting:
        if (!host.hasAnimationForOwner(AnimationOwner::Minigame))
        {
            promptAnimationId = randomItemAnimation();
            queuePromptAnimation();
            state = GuessItemState::WaitingItem;
            lastMoveTime = now;
        }
        break;

    case GuessItemState::WaitingItem:
        if (now - lastMoveTime > kItemRevealDelayMs)
        {
            itemSide = (random(2) == 0) ? GuessItemSide::Left : GuessItemSide::Right;
            state = GuessItemState::WaitingInput;
            lastMoveTime = now;
        }
        break;

    case GuessItemState::WaitingInput:
        if (now - lastMoveTime > kItemPromptSwitchIntervalMs)
        {
            promptAnimationId = randomItemAnimationExcept(promptAnimationId);
            queuePromptAnimation();
            lastMoveTime = now;
        }
        break;

    case GuessItemState::ShowingResult:
        if (host.hasAnimationForOwner(AnimationOwner::Minigame))
            break;

        if (correctCount + wrongCount >= kMaxGuessCount)
        {
#if ENABLE_GUESS_GAME_SINGLE_ROUND
            state = GuessItemState::Inactive;
            lastMoveTime = now;
#else
            state = (correctCount > wrongCount) ? GuessItemState::Win : GuessItemState::Lose;
            const AnimationId finalAnimation = (state == GuessItemState::Win) ? AnimationId::GuessWin : AnimationId::GuessLoss;
            host.clearAnimationsByOwner(AnimationOwner::Minigame);
            host.queueAnimation(Animation(finalAnimation, kResultAnimationDurationMs, true, AnimationOwner::Minigame, AnimationPriority::Critical));
            host.markAnimationDirty();
            lastMoveTime = now;
#endif
        }
        else
        {
            state = GuessItemState::WaitingItem;
            promptAnimationId = randomItemAnimation();
            queuePromptAnimation();
            lastMoveTime = now;
        }
        break;

    case GuessItemState::Cancel:
        if (now - lastMoveTime > kCancelExitDelayMs)
            state = GuessItemState::Inactive;
        break;

    case GuessItemState::Win:
    case GuessItemState::Lose:
        if (!host.hasAnimationForOwner(AnimationOwner::Minigame))
            state = GuessItemState::Inactive;
        break;

    default:
        break;
    }
}

void GuessItemGame::onLeft()
{
    if (state == GuessItemState::WaitingInput)
        handleGuess(GuessItemSide::Left);
}

void GuessItemGame::onRight()
{
    if (state == GuessItemState::WaitingInput)
        handleGuess(GuessItemSide::Right);
}

void GuessItemGame::onMid()
{
    if (!isActive())
        return;

    host.clearAnimationsByOwner(AnimationOwner::Minigame);
    host.markAnimationDirty();
    state = GuessItemState::Cancel;
    lastMoveTime = millis();
}

void GuessItemGame::reset()
{
    correctCount = 0;
    wrongCount = 0;
    itemSide = GuessItemSide::Left;
    promptAnimationId = AnimationId::GuessItem1;
    state = GuessItemState::Inactive;
    lastMoveTime = 0;
}

void GuessItemGame::handleGuess(GuessItemSide player)
{
    const bool correct = (player == itemSide);

    host.clearAnimationsByOwner(AnimationOwner::Minigame);
    host.queueAnimation(Animation(itemResultAnimation(itemSide, player), kResultAnimationDurationMs, true, AnimationOwner::Minigame, AnimationPriority::Critical));
    if (correct)
    {
        host.changePetMood(40);
        correctCount++;
        host.queueAnimation(Animation(AnimationId::GuessRight, kResultAnimationDurationMs, true, AnimationOwner::Minigame, AnimationPriority::Critical));
    }
    else
    {
        host.changePetMood(-5);
        wrongCount++;
        host.queueAnimation(Animation(AnimationId::GuessWrong, kResultAnimationDurationMs, true, AnimationOwner::Minigame, AnimationPriority::Critical));
    }

    host.markAnimationDirty();
    state = GuessItemState::ShowingResult;
    lastMoveTime = millis();
}
