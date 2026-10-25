#include "controller/MinigameController.h"
#include "pet/PetBehaviorRuntime.h"
#if ENABLE_INSTRUCTION_RESPONSE
#include "display/Renderer.h"
#include "common/FirmwareRandom.h"
#endif

#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE

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

void MinigameController::update()
{
#if ENABLE_INSTRUCTION_RESPONSE
    if (activeGame == Kind::Instruction)
    {
        instruction.update(millis());
        if (instruction.active() && !renderInstruction())
        {
            instruction.cancel(); instructionRenderer->showResourceError();
        }
    }
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
    // Predict was a command sequence: its playback time never accrued pet days.
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
#endif // ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE

#if ENABLE_INSTRUCTION_RESPONSE
void MinigameController::startInstruction(Renderer &renderer, const PetBehaviorConfig &config)
{
    reset();
    animations.cancelAll();
    instructionRenderer = &renderer;
    instructionConfig = &config;
    instructionHeld = 7;
    drawnPhase = InstructionResponse::Phase::Idle;
    activeGame = Kind::Instruction;
    auto settings = config.instructionSettings;
    settings.feedbackMs = 500;
    instruction.start(settings, FirmwareRandom::belowWide);
}

bool MinigameController::capturesInstructionInput() const
{
    return activeGame == Kind::Instruction && instruction.active();
}

void MinigameController::instructionInput(uint8_t held, uint8_t pressed, uint32_t timestamp)
{
    instructionHeld = held;
    instruction.input(held, pressed, timestamp);
}

bool MinigameController::renderInstruction()
{
    const auto phase = instruction.phase();
    const uint32_t now = millis();
    Renderer &renderer = *instructionRenderer;
    const bool changed = drawnPhase != phase &&
        !(drawnPhase == InstructionResponse::Phase::Preparing && phase == InstructionResponse::Phase::Answer);
    const auto feedbackRole = instruction.correct() ? FirmwarePlaybackRole::InstructionCorrect :
                                                     FirmwarePlaybackRole::InstructionWrong;
    if (changed)
    {
        renderer.setAnimationArea(0, 32, 128, 96);
        AssetData::AnimationRef prompt = {};
        if (phase == InstructionResponse::Phase::Preparing)
        {
            renderer.clearInstructionOptions();
            for (uint8_t position = 0; position < 3; ++position)
                if (!renderer.ShowAnimationFrame(
                        instructionConfig->instructionOptions[instruction.itemAt(position)],
                        0, 1, position * 43, 128, 12, 42, 32))
                    return false;
            prompt = instructionConfig->systemAnimations[
                static_cast<uint8_t>(instruction.isNoGo() ? FirmwarePlaybackRole::InstructionNoGo1 : FirmwarePlaybackRole::InstructionPrompt1) + instruction.targetItem()];
        }
        else if (phase == InstructionResponse::Phase::Feedback)
            prompt = instructionConfig->systemAnimations[static_cast<uint8_t>(feedbackRole)];
        if (prompt.valid())
        {
            promptVersion = static_cast<uint8_t>(FirmwareRandom::below(renderer.versionCountFor(prompt)));
            frameMs = renderer.frameIntervalFor(prompt, promptVersion, 167);
            if (phase == InstructionResponse::Phase::Feedback)
                instruction.waitForFeedbackAnimation();
            if (!renderer.setAnimation(prompt, promptVersion, phase == InstructionResponse::Phase::Feedback))
                return false;
            renderer.advanceAnimationFrame();
            if (renderer.animationFrameFailed())
                return false;
        }
        else if (phase == InstructionResponse::Phase::Feedback)
            renderer.showInstructionFeedback(instruction.correct());
        else
            return false;
        frameAt = millis();
        drawnPhase = phase;
        if (phase == InstructionResponse::Phase::Preparing || phase == InstructionResponse::Phase::Feedback)
            instruction.ready(millis(), instructionHeld);
        return true;
    }
    drawnPhase = phase;
    const bool animated = phase != InstructionResponse::Phase::Feedback ||
        instructionConfig->systemAnimations[static_cast<uint8_t>(feedbackRole)].valid();
    if (animated && static_cast<uint32_t>(now - frameAt) >= frameMs)
    {
        const bool complete = renderer.advanceAnimationFrame();
        if (renderer.animationFrameFailed())
            return false;
        frameAt = millis();
        if (phase == InstructionResponse::Phase::Feedback && complete)
            instruction.finishFeedbackAnimation();
    }
    return true;
}
#endif
