#include "shared/config/AppProfile.h"

#if ENABLE_GUESS_GAME

#include "minigames/guess_item/domain/GuessItemGame.h"

namespace
{
#ifndef ENABLE_GUESS_GAME_SINGLE_ROUND
#define ENABLE_GUESS_GAME_SINGLE_ROUND 0
#endif

    constexpr unsigned long kItemRevealDelayMs = 50;
    constexpr unsigned long kItemPromptLoopDurationMs = 24UL * 60UL * 60UL * 1000UL;
    constexpr unsigned long kItemPromptSwitchIntervalMs = 800;
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
#if ENABLE_GUESS_GAME_PLAYER_CHOICE_RESULT
        (void)itemSide;
        return (playerSide == GuessItemSide::Left) ? AnimationId::GuessLL : AnimationId::GuessRR;
#else
        constexpr AnimationId kResultByPetAndItem[2][2] = {
            {AnimationId::GuessLL, AnimationId::GuessLR},
            {AnimationId::GuessRL, AnimationId::GuessRR},
        };

        return kResultByPetAndItem[static_cast<int>(playerSide)][static_cast<int>(itemSide)];
#endif
    }
} // namespace

GuessItemGame::GuessItemGame(GuessItemGameHost &hostRef)
    : host(hostRef)
{
    reset();
}

bool GuessItemGame::hasItemPromptAnimations() const
{
    return host.hasAnimation(AnimationId::GuessItem1) &&
           host.hasAnimation(AnimationId::GuessItem2) &&
           host.hasAnimation(AnimationId::GuessItem3) &&
           host.hasAnimation(AnimationId::GuessItem4);
}

void GuessItemGame::queuePromptAnimation(bool replaceExisting)
{
    if (replaceExisting)
        host.clearAnimationsByOwner(AnimationOwner::Minigame);
    host.queueAnimation(Animation(promptAnimationId, kItemPromptLoopDurationMs, false, AnimationOwner::Minigame, AnimationPriority::Critical));
    host.markAnimationDirty();
}

void GuessItemGame::start()
{
    reset();
    host.clearAnimationsByOwner(AnimationOwner::Minigame);

    const bool hasItemPrompts = hasItemPromptAnimations();

    if (host.hasAnimation(AnimationId::GuessStart) && hasItemPrompts)
    {
        promptAnimationId = randomItemAnimation();
        const bool queuedStart = host.queueCompleteAnimation(
            AnimationId::GuessStart, AnimationOwner::Minigame, AnimationPriority::Critical);
        // Queue the first prompt behind GuessStart so playback never falls back
        // to idle between the introductory animation and the user's turn.
        queuePromptAnimation(false);
        state = queuedStart ? GuessItemState::Starting : GuessItemState::WaitingItem;
    }
    else
    {
        promptAnimationId = hasItemPrompts ? randomItemAnimation() : AnimationId::GuessStart;
        queuePromptAnimation();
        state = GuessItemState::WaitingItem;
    }

    lastMoveTime = millis();
}

bool GuessItemGame::isActive() const
{
    return state != GuessItemState::Inactive;
}

void GuessItemGame::update()
{
    if (state == GuessItemState::Inactive)
        return;

    const unsigned long now = millis();
    switch (state)
    {
    case GuessItemState::Starting:
        if (!host.hasAnimationPending(AnimationId::GuessStart))
        {
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
        if (hasItemPromptAnimations() && now - lastMoveTime > kItemPromptSwitchIntervalMs)
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
            const bool won = correctCount > wrongCount;
            host.settleOutcome(won ? GuessItemOutcome::GameWin : GuessItemOutcome::GameLoss);
#if ENABLE_GUESS_GAME_SINGLE_ROUND && !ENABLE_GUESS_GAME_PLAYER_CHOICE_RESULT
            state = GuessItemState::Inactive;
            lastMoveTime = now;
#else
            state = won ? GuessItemState::Win : GuessItemState::Lose;
            const AnimationId finalAnimation = (state == GuessItemState::Win) ? AnimationId::GuessWin : AnimationId::GuessLoss;
            host.clearAnimationsByOwner(AnimationOwner::Minigame);
            if (host.hasAnimation(finalAnimation))
                host.queueCompleteAnimation(finalAnimation, AnimationOwner::Minigame, AnimationPriority::Critical);
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
    if (correct)
    {
        correctCount++;
        host.settleOutcome(GuessItemOutcome::RoundCorrect);
    }
    else
    {
        wrongCount++;
        host.settleOutcome(GuessItemOutcome::RoundWrong);
    }

    host.queueCompleteAnimation(itemResultAnimation(itemSide, player), AnimationOwner::Minigame, AnimationPriority::Critical);
#if !ENABLE_GUESS_GAME_PLAYER_CHOICE_RESULT
    if (correct)
    {
        if (host.hasAnimation(AnimationId::GuessRight))
            host.queueCompleteAnimation(AnimationId::GuessRight, AnimationOwner::Minigame, AnimationPriority::Critical);
    }
    else
    {
        if (host.hasAnimation(AnimationId::GuessWrong))
            host.queueCompleteAnimation(AnimationId::GuessWrong, AnimationOwner::Minigame, AnimationPriority::Critical);
    }
#endif
    host.markAnimationDirty();
    state = GuessItemState::ShowingResult;
    lastMoveTime = millis();
}

#endif // ENABLE_GUESS_GAME
