#ifndef MINIGAME_CONTROLLER_H
#define MINIGAME_CONTROLLER_H

#include "common/AppProfile.h"

#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT || ENABLE_INSTRUCTION_RESPONSE

#include "animation/AnimationController.h"
#if ENABLE_INSTRUCTION_RESPONSE
#include "game/InstructionResponse.h"
class Renderer;
struct PetBehaviorConfig;
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
    void startInstruction(Renderer &renderer, const PetBehaviorConfig &config);
    bool capturesInstructionInput() const;
    void instructionInput(uint8_t held, uint8_t pressed, uint32_t timestamp);
    void instructionPending(bool pending, uint32_t at, uint8_t raw)
    { instruction.pendingInput(pending, at, raw); }
    bool consumeInstructionCompletion(uint32_t &mask, uint8_t &count)
    { return activeGame == Kind::Instruction && instruction.consumeCompletion(mask, count); }
#endif
    void reset();
    void update();
    void onLeft();
    void onRight();
    void onConfirm();
    void onPlaybackFailed();
    bool isActive() const;
    bool pausesPetTime() const;
    bool showsResourceErrorOnPlaybackFailure() const;

private:
    enum class Kind : uint8_t { None, GuessItem, Predict, Instruction };
#if ENABLE_INSTRUCTION_RESPONSE
    InstructionResponse instruction;
    InstructionResponse::Phase drawnPhase = InstructionResponse::Phase::Idle;
    Renderer *instructionRenderer = nullptr;
    const PetBehaviorConfig *instructionConfig = nullptr;
    uint8_t instructionHeld = 7;
    uint8_t promptVersion = 0;
    uint32_t frameAt = 0, frameMs = 167;
    bool renderInstruction();
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
