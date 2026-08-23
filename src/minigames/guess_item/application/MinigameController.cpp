#include "minigames/guess_item/application/MinigameController.h"

#if ENABLE_GUESS_GAME

MinigameController::MinigameController(AnimationController &animationsRef)
    : animations(animationsRef),
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

bool MinigameController::queueCompleteAnimation(AnimationId id,
                                                 AnimationOwner owner,
                                                 AnimationPriority priority)
{
    return animations.queueCompleteAnimation(id, owner, priority);
}

void MinigameController::clearAnimationsByOwner(AnimationOwner owner)
{
    animations.clearByOwner(owner);
}

void MinigameController::markAnimationDirty()
{
    animations.markDirty();
}

bool MinigameController::hasAnimation(AnimationId id) const
{
    return animations.hasAnimation(id);
}

bool MinigameController::hasAnimationPending(AnimationId id) const
{
    return animations.hasAnimationPending(id);
}

bool MinigameController::hasAnimationForOwner(AnimationOwner owner) const
{
    return animations.hasAnimationForOwner(owner);
}

#endif // ENABLE_GUESS_GAME
