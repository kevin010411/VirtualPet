#ifndef MINIGAME_CONTROLLER_H
#define MINIGAME_CONTROLLER_H

#include "common/AppProfile.h"

#if ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT

#include "animation/AnimationController.h"
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
    enum class Kind : uint8_t { None, GuessItem, Predict };
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

#endif // ENABLE_GUESS_GAME || ENABLE_COMMAND_PREDICT

#endif // MINIGAME_CONTROLLER_H
