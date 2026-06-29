#include "app/MinigameController.h"

#if ENABLE_GUESS_ITEM_GAME

MinigameController::MinigameController(PetActionController &petActionsRef, AnimationController &animationsRef)
    : petActions(petActionsRef),
      animations(animationsRef),
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

bool MinigameController::isActive() const
{
    return guessItem.isActive();
}

void MinigameController::queueAnimation(const Animation &animation)
{
    animations.queueAnimation(animation);
}

void MinigameController::clearAnimationsByOwner(AnimationOwner owner)
{
    animations.clearByOwner(owner);
}

void MinigameController::markAnimationDirty()
{
    animations.markDirty();
}

void MinigameController::changePetMood(int delta)
{
    petActions.changeMood(delta);
}

bool MinigameController::hasAnimation(AnimationId id) const
{
    return animations.hasAnimation(id);
}

bool MinigameController::hasAnimationForOwner(AnimationOwner owner) const
{
    return animations.hasAnimationForOwner(owner);
}

#endif // ENABLE_GUESS_ITEM_GAME
