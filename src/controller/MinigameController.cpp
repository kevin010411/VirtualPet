#include "controller/MinigameController.h"
#include "pet/PetBehaviorRuntime.h"

#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE || ENABLE_CAPTURE_GAME

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

#if ENABLE_CAPTURE_GAME
bool MinigameController::startCapture(Renderer &renderer, AssetResources &resources, CaptureOutcomeHost &outcomes)
{
    reset();
    animations.cancelAll();
    if (!capture.start(renderer, resources, outcomes)) return false;
    activeGame = Kind::Capture;
    return true;
}
#endif

void MinigameController::reset()
{
#if ENABLE_CAPTURE_GAME
    capture.cancel();
#endif
#if ENABLE_INSTRUCTION_RESPONSE
    instruction.cancel();
#endif
    activeGame = Kind::None;
#if ENABLE_GUESS_GAME
    guessItem.reset();
#endif
#if ENABLE_COMMAND_PREDICT
    predict.reset();
#endif
}

void MinigameController::update(bool confirmPending)
{
#if ENABLE_CAPTURE_GAME
    if (activeGame == Kind::Capture) capture.update(confirmPending);
#else
    (void)confirmPending;
#endif
#if ENABLE_INSTRUCTION_RESPONSE
    if (activeGame == Kind::Instruction) instruction.update();
#endif
#if ENABLE_GUESS_GAME
    if (activeGame == Kind::GuessItem) guessItem.update();
#endif
#if ENABLE_COMMAND_PREDICT
    if (activeGame == Kind::Predict) predict.update();
#endif
}

void MinigameController::onLeft()
{
#if ENABLE_CAPTURE_GAME
    if (activeGame == Kind::Capture) capture.move(-1);
#endif
#if ENABLE_GUESS_GAME
    if (activeGame == Kind::GuessItem) guessItem.onLeft();
#endif
}

void MinigameController::onRight()
{
#if ENABLE_CAPTURE_GAME
    if (activeGame == Kind::Capture) capture.move(1);
#endif
#if ENABLE_GUESS_GAME
    if (activeGame == Kind::GuessItem) guessItem.onRight();
#endif
}

void MinigameController::onConfirm()
{
#if ENABLE_CAPTURE_GAME
    if (activeGame == Kind::Capture) capture.confirm();
#endif
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
#if ENABLE_CAPTURE_GAME
    if (activeGame == Kind::Capture) return capture.active();
#endif
#if ENABLE_INSTRUCTION_RESPONSE
    if (activeGame == Kind::Instruction) return instruction.active();
#endif
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
    // Predict and Instruction retain their existing playback-clock policy.
    return activeGame == Kind::Predict || activeGame == Kind::Instruction;
}

bool MinigameController::showsResourceErrorOnPlaybackFailure() const
{
    return activeGame == Kind::Predict || activeGame == Kind::Instruction;
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
#endif // ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE || ENABLE_CAPTURE_GAME

#if ENABLE_INSTRUCTION_RESPONSE
void MinigameController::startInstruction(Renderer &renderer, AssetResources &resources, const PetBehaviorConfig &config)
{
    reset();
    animations.cancelAll();
    instruction.start(renderer, resources, config);
    activeGame = Kind::Instruction;
}

bool MinigameController::capturesInstructionInput() const
{
    return activeGame == Kind::Instruction && instruction.active();
}

void MinigameController::instructionInput(uint8_t held, uint8_t pressed, uint32_t timestamp)
{
    instruction.input(held, pressed, timestamp);
}

#endif
