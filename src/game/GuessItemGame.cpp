#include "common/AppProfile.h"

#if ENABLE_GUESS_GAME

#include "game/GuessItemGame.h"
#include "common/FirmwareRandom.h"

namespace
{
    constexpr unsigned long kItemRevealDelayMs = 50;
    constexpr unsigned long kItemPromptLoopDurationMs = 24UL * 60UL * 60UL * 1000UL;
    constexpr unsigned long kItemPromptSwitchIntervalMs = 800;
    constexpr unsigned long kCancelExitDelayMs = 250;
#if ENABLE_GUESS_GAME_SINGLE_ROUND
    constexpr int kMaxGuessCount = 1;
#else
    constexpr int kMaxGuessCount = 3;
#endif

    constexpr FirmwarePlaybackRole kItemPrompts[] = {
        FirmwarePlaybackRole::GuessItem1,
        FirmwarePlaybackRole::GuessItem2,
        FirmwarePlaybackRole::GuessItem3,
        FirmwarePlaybackRole::GuessItem4,
    };

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

FirmwarePlaybackRole GuessItemGame::randomItemAnimation() const
{
    return itemPromptCount ? kItemPrompts[FirmwareRandom::below(itemPromptCount)] : FirmwarePlaybackRole::GuessStart;
}

FirmwarePlaybackRole GuessItemGame::nextItemAnimation() const
{
    uint8_t index = static_cast<uint8_t>(FirmwareRandom::below(itemPromptCount - 1));
    for (uint8_t skipped = 0; skipped <= index; ++skipped)
        if (kItemPrompts[skipped] == promptPlaybackRole)
        {
            ++index;
            break;
        }
    return kItemPrompts[index];
}

PlaybackResult GuessItemGame::replacePromptAnimation()
{
    const Animation prompt(promptPlaybackRole, kItemPromptLoopDurationMs, false);
    return host.replace(AnimationSequence(&prompt, 1));
}

void GuessItemGame::start()
{
    reset();

    // The exporter assigns the selected animations to a dense prefix of roles.
    for (const auto role : kItemPrompts)
    {
        if (!host.hasAnimation(role))
            break;
        ++itemPromptCount;
    }

    promptPlaybackRole = randomItemAnimation();
    if (host.hasAnimation(FirmwarePlaybackRole::GuessStart) && itemPromptCount)
    {
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
            itemSide = (FirmwareRandom::below(2) == 0) ? GuessItemSide::Left : GuessItemSide::Right;
            state = GuessItemState::WaitingInput;
            lastMoveTime = now;
        }
        break;

    case GuessItemState::WaitingInput:
        if (itemPromptCount > 1 && now - lastMoveTime > kItemPromptSwitchIntervalMs)
        {
            promptPlaybackRole = nextItemAnimation();
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
            if (state == GuessItemState::Inactive) return;
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
    promptPlaybackRole = FirmwarePlaybackRole::GuessStart;
    itemPromptCount = 0;
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
        if (state == GuessItemState::Inactive) return;
    }
    else
    {
        wrongCount++;
        host.settleOutcome(GuessItemOutcome::RoundWrong);
        if (state == GuessItemState::Inactive) return;
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
        if (state == GuessItemState::Inactive) return;
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
