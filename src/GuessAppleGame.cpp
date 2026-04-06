#include "GuessAppleGame.h"

namespace
{
constexpr unsigned long kAppleRevealDelayMs = 300;
constexpr unsigned long kApplePromptLoopDurationMs = 24UL * 60UL * 60UL * 1000UL;
constexpr unsigned long kResultAnimationDurationMs = 2000;
constexpr unsigned long kExitDelayMs = 250;

AnimationId randomAppleAnimation()
{
    switch (random(1, 4))
    {
    case 1:
        return AnimationId::Apple1;
    case 2:
        return AnimationId::Apple2;
    default:
        return AnimationId::Apple3;
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
    promptAnimationId = randomAppleAnimation();
    queuePromptAnimation();
    state = GuessAppleState::WaitingApple;
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
    case GuessAppleState::WaitingApple:
        if (now - lastMoveTime > kAppleRevealDelayMs)
        {
            appleSide = (random(2) == 0) ? GuessAppleSide::Left : GuessAppleSide::Right;
            state = GuessAppleState::WaitingInput;
        }
        break;

    case GuessAppleState::ShowingResult:
        if (correctCount >= 3)
        {
            state = GuessAppleState::Win;
            host.queueAnimation(Animation(AnimationId::GuessWin, kResultAnimationDurationMs, true, AnimationOwner::Minigame, AnimationPriority::Critical));
            host.markAnimationDirty();
        }
        else if (wrongCount >= 3)
        {
            state = GuessAppleState::Lose;
            host.queueAnimation(Animation(AnimationId::GuessLoss, kResultAnimationDurationMs, true, AnimationOwner::Minigame, AnimationPriority::Critical));
            host.markAnimationDirty();
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
    case GuessAppleState::Win:
    case GuessAppleState::Lose:
        if (now - lastMoveTime > kExitDelayMs)
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
