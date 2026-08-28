#include "minigames/guess_item/application/MinigameController.h"
#include "pet_behavior/application/PetBehaviorRuntime.h"

#if ENABLE_GUESS_GAME

MinigameController::MinigameController(AnimationController &animationsRef,
                                       PetBehaviorRuntime &petBehaviorRef)
    : animations(animationsRef),
      petBehavior(petBehaviorRef),
      guessItem(*this)
{
}

void MinigameController::startGuessItem()
{
    guessItem.start();
}

void MinigameController::reset()
{
    guessItem.reset();
}

void MinigameController::update()
{
    guessItem.update();
}

void MinigameController::onLeft()
{
    guessItem.onLeft();
}

void MinigameController::onRight()
{
    guessItem.onRight();
}

void MinigameController::onConfirm()
{
    guessItem.onMid();
}

void MinigameController::onPlaybackFailed()
{
    guessItem.onPlaybackFailed();
}

bool MinigameController::isActive() const
{
    return guessItem.isActive();
}

PlaybackResult MinigameController::submit(const AnimationSequence &sequence, PlaybackMode mode)
{
    return animations.submit(sequence, mode);
}

PlaybackResult MinigameController::buildCompleteAnimation(AnimationId id, Animation &animation) const
{
    return animations.buildCompleteAnimation(id, animation);
}

void MinigameController::cancelPlayback()
{
    animations.cancelAll();
}

bool MinigameController::isPlaybackBusy() const
{
    return animations.isBusy();
}

bool MinigameController::hasAnimation(AnimationId id) const
{
    return animations.hasAnimation(id);
}

bool MinigameController::hasAnimationPending(AnimationId id) const
{
    return animations.hasAnimationPending(id);
}

void MinigameController::settleOutcome(GuessItemOutcome outcome)
{
    PetBehaviorGuessOutcome behaviorOutcome = PetBehaviorGuessOutcome::RoundCorrect;
    switch (outcome)
    {
    case GuessItemOutcome::RoundWrong:
        behaviorOutcome = PetBehaviorGuessOutcome::RoundWrong;
        break;
    case GuessItemOutcome::GameWin:
        behaviorOutcome = PetBehaviorGuessOutcome::GameWin;
        break;
    case GuessItemOutcome::GameLoss:
        behaviorOutcome = PetBehaviorGuessOutcome::GameLoss;
        break;
    default:
        break;
    }
    petBehavior.applyGuessOutcome(behaviorOutcome);
}

#endif // ENABLE_GUESS_GAME
