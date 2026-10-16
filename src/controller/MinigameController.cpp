#include "controller/MinigameController.h"
#include "pet/PetBehaviorRuntime.h"

#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT

MinigameController::MinigameController(AnimationController &animationsRef
#if ENABLE_GUESS_GAME
                                       , PetBehaviorRuntime &petBehaviorRef,
                                       MinigameOutcomeHost &outcomeHostRef
#endif
    )
    : animations(animationsRef)
#if ENABLE_COMMAND_PREDICT
      , predict(animationsRef)
#endif
#if ENABLE_GUESS_GAME
      , petBehavior(petBehaviorRef),
      outcomeHost(outcomeHostRef),
      guessItem(*this)
#endif
{
}

#if ENABLE_GUESS_GAME
void MinigameController::startGuessItem()
{
    reset();
    activeGame = Kind::GuessItem;
    guessItem.start();
}
#endif

#if ENABLE_COMMAND_PREDICT
bool MinigameController::startPredict()
{
    reset();
    if (!predict.start()) return false;
    activeGame = Kind::Predict;
    return true;
}
#endif

void MinigameController::reset()
{
    activeGame = Kind::None;
#if ENABLE_GUESS_GAME
    guessItem.reset();
#endif
#if ENABLE_COMMAND_PREDICT
    predict.reset();
#endif
}

void MinigameController::update()
{
#if ENABLE_GUESS_GAME
    if (activeGame == Kind::GuessItem) guessItem.update();
#endif
#if ENABLE_COMMAND_PREDICT
    if (activeGame == Kind::Predict) predict.update();
#endif
}

void MinigameController::onLeft()
{
#if ENABLE_GUESS_GAME
    if (activeGame == Kind::GuessItem) guessItem.onLeft();
#endif
}

void MinigameController::onRight()
{
#if ENABLE_GUESS_GAME
    if (activeGame == Kind::GuessItem) guessItem.onRight();
#endif
}

void MinigameController::onConfirm()
{
#if ENABLE_GUESS_GAME
    if (activeGame == Kind::GuessItem) guessItem.onMid();
#endif
}

void MinigameController::onPlaybackFailed()
{
#if ENABLE_GUESS_GAME
    if (activeGame == Kind::GuessItem) guessItem.onPlaybackFailed();
#endif
#if ENABLE_COMMAND_PREDICT
    if (activeGame == Kind::Predict) predict.onPlaybackFailed();
#endif
}

bool MinigameController::isActive() const
{
#if ENABLE_GUESS_GAME
    if (activeGame == Kind::GuessItem) return guessItem.isActive();
#endif
#if ENABLE_COMMAND_PREDICT
    if (activeGame == Kind::Predict) return predict.isActive();
#endif
    return false;
}

bool MinigameController::pausesPetTime() const
{
    // Predict was a command sequence: its playback time never accrued pet days.
    return activeGame == Kind::Predict;
}

bool MinigameController::showsResourceErrorOnPlaybackFailure() const
{
    return activeGame == Kind::Predict;
}

#if ENABLE_GUESS_GAME
PlaybackResult MinigameController::replace(const AnimationSequence &sequence)
{
    return animations.replace(sequence);
}

void MinigameController::cancelPlayback()
{
    animations.cancelAll();
}

bool MinigameController::isPlaybackBusy() const
{
    return animations.isBusy();
}

bool MinigameController::hasAnimation(FirmwarePlaybackRole id) const
{
    return animations.hasAnimation(id);
}

bool MinigameController::hasAnimationPending(FirmwarePlaybackRole id) const
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
    if (petBehavior.applyGuessOutcome(behaviorOutcome))
        outcomeHost.onGuessOutcomeApplied();
}

#endif // ENABLE_GUESS_GAME
#endif // ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT
