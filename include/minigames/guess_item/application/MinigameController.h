#ifndef MINIGAME_CONTROLLER_H
#define MINIGAME_CONTROLLER_H

#include "shared/config/AppProfile.h"

#if ENABLE_GUESS_ITEM_GAME

#include "animation/application/AnimationController.h"
#include "pet/application/PetActionController.h"
#include "minigames/guess_item/domain/GuessItemGame.h"

class MinigameController : public GuessItemGameHost
{
public:
    MinigameController(PetActionController &petActions, AnimationController &animations);

    void startGuessItem();
    void reset();
    void update();
    void onLeft();
    void onRight();
    void onConfirm();
    bool isActive() const;

private:
    PetActionController &petActions;
    AnimationController &animations;
    GuessItemGame guessItem;

    void queueAnimation(const Animation &animation) override;
    void clearAnimationsByOwner(AnimationOwner owner) override;
    void markAnimationDirty() override;
    void changePetMood(int delta) override;
    bool hasAnimation(AnimationId id) const override;
    bool hasAnimationForOwner(AnimationOwner owner) const override;
};

#endif // ENABLE_GUESS_ITEM_GAME

#endif // MINIGAME_CONTROLLER_H
