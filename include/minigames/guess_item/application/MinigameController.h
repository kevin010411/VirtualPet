#ifndef MINIGAME_CONTROLLER_H
#define MINIGAME_CONTROLLER_H

#include "shared/config/AppProfile.h"

#if ENABLE_GUESS_GAME

#include "animation/application/AnimationController.h"
#include "minigames/guess_item/domain/GuessItemGame.h"

class MinigameController : public GuessItemGameHost
{
public:
    explicit MinigameController(AnimationController &animations);

    void startGuessItem();
    void reset();
    void update();
    void onLeft();
    void onRight();
    void onConfirm();
    bool isActive() const;

private:
    AnimationController &animations;
    GuessItemGame guessItem;

    void queueAnimation(const Animation &animation) override;
    void clearAnimationsByOwner(AnimationOwner owner) override;
    void markAnimationDirty() override;
    bool hasAnimation(AnimationId id) const override;
    bool hasAnimationForOwner(AnimationOwner owner) const override;
};

#endif // ENABLE_GUESS_GAME

#endif // MINIGAME_CONTROLLER_H
