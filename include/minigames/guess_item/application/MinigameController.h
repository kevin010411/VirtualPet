#ifndef MINIGAME_CONTROLLER_H
#define MINIGAME_CONTROLLER_H

#include "shared/config/AppProfile.h"

#if ENABLE_GUESS_GAME

#include "animation/application/AnimationController.h"
#include "minigames/guess_item/domain/GuessItemGame.h"

class PetBehaviorRuntime;

class MinigameController : public GuessItemGameHost
{
public:
    MinigameController(AnimationController &animations, PetBehaviorRuntime &petBehavior);

    void startGuessItem();
    void reset();
    void update();
    void onLeft();
    void onRight();
    void onConfirm();
    void onPlaybackFailed();
    bool isActive() const;

private:
    AnimationController &animations;
    PetBehaviorRuntime &petBehavior;
    GuessItemGame guessItem;

    PlaybackResult submit(const AnimationSequence &sequence, PlaybackMode mode) override;
    void cancelPlayback() override;
    bool isPlaybackBusy() const override;
    bool hasAnimation(AnimationId id) const override;
    bool hasAnimationPending(AnimationId id) const override;
    void settleOutcome(GuessItemOutcome outcome) override;
};

#endif // ENABLE_GUESS_GAME

#endif // MINIGAME_CONTROLLER_H
