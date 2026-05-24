#include "GuessAppleGame.h"

namespace
{
    constexpr unsigned long kAppleRevealDelayMs = 50;
    constexpr unsigned long kStartAnimationDurationMs = 1200;
    constexpr unsigned long kApplePromptLoopDurationMs = 24UL * 60UL * 60UL * 1000UL;
    constexpr unsigned long kResultAnimationDurationMs = 2000;
    constexpr unsigned long kCancelExitDelayMs = 250;
    constexpr int kMaxGuessCount = 3;

    AnimationId randomAppleAnimation()
    {
        switch (random(1, 5))
        {
        case 1:
            return AnimationId::Apple1;
        case 2:
            return AnimationId::Apple2;
        case 3:
            return AnimationId::Apple3;
        default:
            return AnimationId::Apple4;
        }
    }

    AnimationId appleResultAnimation(GuessAppleSide appleSide, GuessAppleSide playerSide)
    {
        const int result = static_cast<int>(appleSide) + static_cast<int>(playerSide) * 2 + 1;
        switch (result)
        {
        case 1:
            return AnimationId::AppleP1;
        case 2:
            return AnimationId::AppleP2;
        case 3:
            return AnimationId::AppleP3;
        default:
            return AnimationId::AppleP4;
        }
    }
} // namespace

GuessAppleGame::GuessAppleGame(GuessAppleGameHost &hostRef)
    : host(hostRef)
{
    reset();
}

void GuessAppleGame::queuePromptAnimation()
{
    host.clearAnimationsByOwner(AnimationOwner::Minigame);
    host.queueAnimation(Animation(promptAnimationId, kApplePromptLoopDurationMs, false, AnimationOwner::Minigame, AnimationPriority::Critical));
    host.markAnimationDirty();
}

void GuessAppleGame::start()
{
    reset();
    host.clearAnimationsByOwner(AnimationOwner::Minigame);
    host.queueAnimation(Animation(AnimationId::GuessStart, kStartAnimationDurationMs, true, AnimationOwner::Minigame, AnimationPriority::Critical));
    host.markAnimationDirty();
    state = GuessAppleState::Starting;
    lastMoveTime = millis();
}

bool GuessAppleGame::isActive() const
{
    return state != GuessAppleState::Inactive;
}

bool GuessAppleGame::isFinished() const
{
    return state == GuessAppleState::Win || state == GuessAppleState::Lose || state == GuessAppleState::Cancel;
}

bool GuessAppleGame::isWin() const
{
    return state == GuessAppleState::Win;
}

void GuessAppleGame::update()
{
    if (state == GuessAppleState::Inactive)
        return;

    const unsigned long now = millis();
    switch (state)
    {
    case GuessAppleState::Starting:
        if (!host.hasAnimationForOwner(AnimationOwner::Minigame))
        {
            promptAnimationId = randomAppleAnimation();
            queuePromptAnimation();
            state = GuessAppleState::WaitingApple;
            lastMoveTime = now;
        }
        break;

    case GuessAppleState::WaitingApple:
        if (now - lastMoveTime > kAppleRevealDelayMs)
        {
            appleSide = (random(2) == 0) ? GuessAppleSide::Left : GuessAppleSide::Right;
            state = GuessAppleState::WaitingInput;
        }
        break;

    case GuessAppleState::ShowingResult:
        if (host.hasAnimationForOwner(AnimationOwner::Minigame))
            break;

        if (correctCount + wrongCount >= kMaxGuessCount)
        {
            state = (correctCount > wrongCount) ? GuessAppleState::Win : GuessAppleState::Lose;
            const AnimationId finalAnimation = (state == GuessAppleState::Win) ? AnimationId::GuessWin : AnimationId::GuessLoss;
            host.clearAnimationsByOwner(AnimationOwner::Minigame);
            host.queueAnimation(Animation(finalAnimation, kResultAnimationDurationMs, true, AnimationOwner::Minigame, AnimationPriority::Critical));
            host.markAnimationDirty();
            lastMoveTime = now;
        }
        else
        {
            state = GuessAppleState::WaitingApple;
            promptAnimationId = randomAppleAnimation();
            queuePromptAnimation();
            lastMoveTime = now;
        }
        break;

    case GuessAppleState::Cancel:
        if (now - lastMoveTime > kCancelExitDelayMs)
            state = GuessAppleState::Inactive;
        break;

    case GuessAppleState::Win:
    case GuessAppleState::Lose:
        if (!host.hasAnimationForOwner(AnimationOwner::Minigame))
            state = GuessAppleState::Inactive;
        break;

    case GuessAppleState::WaitingInput:
    default:
        break;
    }
}

void GuessAppleGame::onLeft()
{
    if (state == GuessAppleState::WaitingInput)
        handleGuess(GuessAppleSide::Left);
}

void GuessAppleGame::onRight()
{
    if (state == GuessAppleState::WaitingInput)
        handleGuess(GuessAppleSide::Right);
}

void GuessAppleGame::onMid()
{
    if (!isActive())
        return;

    host.clearAnimationsByOwner(AnimationOwner::Minigame);
    host.markAnimationDirty();
    state = GuessAppleState::Cancel;
    lastMoveTime = millis();
}

void GuessAppleGame::reset()
{
    correctCount = 0;
    wrongCount = 0;
    appleSide = GuessAppleSide::Left;
    promptAnimationId = AnimationId::Apple1;
    state = GuessAppleState::Inactive;
    lastMoveTime = 0;
}

void GuessAppleGame::handleGuess(GuessAppleSide player)
{
    const bool correct = (player == appleSide);

    host.clearAnimationsByOwner(AnimationOwner::Minigame);
    host.queueAnimation(Animation(appleResultAnimation(appleSide, player), kResultAnimationDurationMs, true, AnimationOwner::Minigame, AnimationPriority::Critical));
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
    state = GuessAppleState::ShowingResult;
    lastMoveTime = millis();
}
