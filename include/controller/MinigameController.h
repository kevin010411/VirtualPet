#ifndef MINIGAME_CONTROLLER_H
#define MINIGAME_CONTROLLER_H

#include "common/AppProfile.h"

#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE || ENABLE_CAPTURE_GAME

#include "animation/AnimationController.h"
#if ENABLE_CAPTURE_GAME
#include "game/CaptureGame.h"
#endif
#if ENABLE_INSTRUCTION_RESPONSE
#include "game/InstructionPresentation.h"
struct InstructionRuntimeSettings;
struct RuntimePlaybackConfig;
#endif
#if ENABLE_CAPTURE_GAME || ENABLE_INSTRUCTION_RESPONSE
class Renderer;
class AssetResources;
#endif
#if ENABLE_COMMAND_PREDICT
#include "game/PredictGame.h"
#endif
#if ENABLE_GUESS_GAME
#include "game/GuessItemGame.h"

class PetBehaviorRuntime;

class MinigameOutcomeHost
{
public:
    virtual void onGuessOutcomeApplied() = 0;
protected:
    ~MinigameOutcomeHost() = default;
};
#endif

class MinigameController
#if ENABLE_GUESS_GAME
    : public GuessItemGameHost
#endif
{
public:
    explicit MinigameController(AnimationController &animations
#if ENABLE_GUESS_GAME
                       , PetBehaviorRuntime &petBehavior, MinigameOutcomeHost &outcomeHost
#endif
    );

#if ENABLE_GUESS_GAME
    void startGuessItem();
#endif
#if ENABLE_COMMAND_PREDICT
    bool startPredict();
#endif
#if ENABLE_INSTRUCTION_RESPONSE
    void startInstruction(Renderer &renderer, AssetResources &resources, const InstructionRuntimeSettings &config,
                          const RuntimePlaybackConfig &playback);
    bool capturesInstructionInput() const;
    void instructionInput(uint8_t held, uint8_t pressed, uint32_t timestamp);
    void instructionPending(bool pending, uint32_t at, uint8_t raw)
    { instruction.pendingInput(pending, at, raw); }
    bool consumeInstructionCompletion(uint32_t &mask, uint8_t &count)
    { return activeGame == Kind::Instruction && instruction.consumeCompletion(mask, count); }
#endif
#if ENABLE_CAPTURE_GAME
    bool startCapture(Renderer &renderer, AssetResources &resources, CaptureOutcomeHost &outcomes);
    bool capturesScreen() const { return activeGame == Kind::Capture && capture.active(); }
    bool captureClockRunning() const { return activeGame == Kind::Capture && capture.clockRunning(); }
    void redrawCapture() { capture.redraw(); }
#endif
    void reset();
    void update(bool confirmPending = false);
    void onLeft();
    void onRight();
    void onConfirm();
    void onPlaybackFailed();
    bool isActive() const;
    bool pausesPetTime() const;
    bool showsResourceErrorOnPlaybackFailure() const;

private:
    enum class Kind : uint8_t { None, GuessItem, Predict, Instruction, Capture };
#if ENABLE_CAPTURE_GAME
    CaptureGame capture;
#endif
#if ENABLE_INSTRUCTION_RESPONSE
    InstructionPresentation instruction;
#endif
    Kind activeGame = Kind::None;
    AnimationController &animations;
#if ENABLE_COMMAND_PREDICT
    PredictGame predict;
#endif
#if ENABLE_GUESS_GAME
    PetBehaviorRuntime &petBehavior;
    MinigameOutcomeHost &outcomeHost;
    GuessItemGame guessItem;

    PlaybackResult replace(const AnimationSequence &sequence) override;
    void cancelPlayback() override;
    bool isPlaybackBusy() const override;
    bool hasAnimation(FirmwarePlaybackRole id) const override;
    bool hasAnimationPending(FirmwarePlaybackRole id) const override;
    void settleOutcome(GuessItemOutcome outcome) override;
#endif
};

#endif // ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE

#endif // MINIGAME_CONTROLLER_H
