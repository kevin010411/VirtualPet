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

    FirmwarePlaybackRole randomItemAnimation()
    {
        switch (random(1, 5))
        {
        case 1:
            return FirmwarePlaybackRole::GuessItem1;
        case 2:
            return FirmwarePlaybackRole::GuessItem2;
        case 3:
            return FirmwarePlaybackRole::GuessItem3;
        default:
            return FirmwarePlaybackRole::GuessItem4;
        }
    }

    FirmwarePlaybackRole randomItemAnimationExcept(FirmwarePlaybackRole current)
    {
        FirmwarePlaybackRole next = randomItemAnimation();
        while (next == current)
            next = randomItemAnimation();

        return next;
    }

    FirmwarePlaybackRole itemResultAnimation(GuessItemSide itemSide, GuessItemSide playerSide)
    {
#if ENABLE_GUESS_GAME_PLAYER_CHOICE_RESULT
        (void)itemSide;
        return (playerSide == GuessItemSide::Left) ? FirmwarePlaybackRole::GuessLL : FirmwarePlaybackRole::GuessRR;
#else
        constexpr FirmwarePlaybackRole kResultByPetAndItem[2][2] = {
            {FirmwarePlaybackRole::GuessLL, FirmwarePlaybackRole::GuessLR},
            {FirmwarePlaybackRole::GuessRL, FirmwarePlaybackRole::GuessRR},
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
    return host.hasAnimation(FirmwarePlaybackRole::GuessItem1) &&
           host.hasAnimation(FirmwarePlaybackRole::GuessItem2) &&
           host.hasAnimation(FirmwarePlaybackRole::GuessItem3) &&
           host.hasAnimation(FirmwarePlaybackRole::GuessItem4);
}

PlaybackResult GuessItemGame::replacePromptAnimation()
{
    const Animation prompt(promptPlaybackRole, kItemPromptLoopDurationMs, false);
    return host.replace(AnimationSequence(&prompt, 1));
}

void GuessItemGame::start()
{
    reset();

    const bool hasItemPrompts = hasItemPromptAnimations();

    if (host.hasAnimation(FirmwarePlaybackRole::GuessStart) && hasItemPrompts)
    {
        promptPlaybackRole = randomItemAnimation();
        const Animation sequence[] = {
            Animation::complete(FirmwarePlaybackRole::GuessStart),
            Animation(promptPlaybackRole, kItemPromptLoopDurationMs, false),
        };
        // Keep the playable start and prompt items adjacent so playback does not
        // fall back to idle between the introduction and the user's turn.
        const PlaybackResult result = host.replace(AnimationSequence(sequence, 2));
        if (result == PlaybackResult::Accepted)
        {
            state = GuessItemState::Starting;
        }
        else
        {
            replacePromptAnimation();
            state = GuessItemState::WaitingItem;
        }
    }
    else
    {
        promptPlaybackRole = hasItemPrompts ? randomItemAnimation() : FirmwarePlaybackRole::GuessStart;
        replacePromptAnimation();
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
        if (!host.hasAnimationPending(FirmwarePlaybackRole::GuessStart))
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
            promptPlaybackRole = randomItemAnimationExcept(promptPlaybackRole);
            replacePromptAnimation();
            lastMoveTime = now;
        }
        break;

    case GuessItemState::ShowingResult:
        if (host.isPlaybackBusy())
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
            const FirmwarePlaybackRole finalAnimation = (state == GuessItemState::Win) ? FirmwarePlaybackRole::GuessWin : FirmwarePlaybackRole::GuessLoss;
            host.cancelPlayback();
            if (host.hasAnimation(finalAnimation))
            {
                const Animation animation = Animation::complete(finalAnimation);
                host.replace(AnimationSequence(&animation, 1));
            }
            lastMoveTime = now;
#endif
        }
        else
        {
            state = GuessItemState::WaitingItem;
            promptPlaybackRole = randomItemAnimation();
            replacePromptAnimation();
            lastMoveTime = now;
        }
        break;

    case GuessItemState::Cancel:
        if (now - lastMoveTime > kCancelExitDelayMs)
            state = GuessItemState::Inactive;
        break;

    case GuessItemState::Win:
    case GuessItemState::Lose:
        if (!host.isPlaybackBusy())
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

    host.cancelPlayback();
    state = GuessItemState::Cancel;
    lastMoveTime = millis();
}

void GuessItemGame::onPlaybackFailed()
{
    host.cancelPlayback();
    if (state == GuessItemState::Starting)
    {
        replacePromptAnimation();
        state = GuessItemState::WaitingItem;
        lastMoveTime = millis();
    }
    else if (state == GuessItemState::Win || state == GuessItemState::Lose)
    {
        state = GuessItemState::Inactive;
    }
}

void GuessItemGame::reset()
{
    correctCount = 0;
    wrongCount = 0;
    itemSide = GuessItemSide::Left;
    promptPlaybackRole = FirmwarePlaybackRole::GuessItem1;
    state = GuessItemState::Inactive;
    lastMoveTime = 0;
}

void GuessItemGame::handleGuess(GuessItemSide player)
{
    const bool correct = (player == itemSide);

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

    Animation sequence[3];
    uint8_t sequenceCount = 0;
    const FirmwarePlaybackRole itemResult = itemResultAnimation(itemSide, player);
    if (host.hasAnimation(itemResult))
    {
        sequence[sequenceCount] = Animation::complete(itemResult);
        ++sequenceCount;
    }
#if !ENABLE_GUESS_GAME_PLAYER_CHOICE_RESULT
    if (correct)
    {
        if (host.hasAnimation(FirmwarePlaybackRole::GuessRight))
        {
            sequence[sequenceCount] = Animation::complete(FirmwarePlaybackRole::GuessRight);
            ++sequenceCount;
        }
    }
    else
    {
        if (host.hasAnimation(FirmwarePlaybackRole::GuessWrong))
        {
            sequence[sequenceCount] = Animation::complete(FirmwarePlaybackRole::GuessWrong);
            ++sequenceCount;
        }
    }
#endif

    const bool isFinalRound = correctCount + wrongCount >= kMaxGuessCount;
    GuessItemState nextState = GuessItemState::ShowingResult;
    if (isFinalRound)
    {
#if !(ENABLE_GUESS_GAME_SINGLE_ROUND && !ENABLE_GUESS_GAME_PLAYER_CHOICE_RESULT)
        const bool won = correctCount > wrongCount;
        const FirmwarePlaybackRole finalAnimation = won ? FirmwarePlaybackRole::GuessWin : FirmwarePlaybackRole::GuessLoss;
        // Keep the final outcome adjacent to LL/LR/RL/RR (and optional
        // GuessRight/GuessWrong) so no waiting state separates playback.
        if (host.hasAnimation(finalAnimation))
        {
            sequence[sequenceCount] = Animation::complete(finalAnimation);
            ++sequenceCount;
        }
        host.settleOutcome(won ? GuessItemOutcome::GameWin : GuessItemOutcome::GameLoss);
        nextState = won ? GuessItemState::Win : GuessItemState::Lose;
#endif
    }

    if (sequenceCount > 0)
        host.replace(AnimationSequence(sequence, sequenceCount));
    else
        host.cancelPlayback();
    state = nextState;
    lastMoveTime = millis();
}

#endif // ENABLE_GUESS_GAME
