#include "animation/application/AnimationController.h"

#include <string.h>
#include "presentation/adapters/rendering/Renderer.h"

AnimationController::AnimationController(Renderer &rendererRef)
    : renderer(rendererRef)
{
}

void AnimationController::setup(AnimationId baseAnimation)
{
    renderer.initAnimations();
    animationQueue.clear();
    activeAnimation = Animation();
    hasActiveAnimation = false;
    baseAnimationId = baseAnimation;
    displayDuration = 0;
    dirtyAnimation = true;
    animateDone = true;
    frameInterval = frameIntervalSlow;
    lastFrameTime = 0;
    showAnimationId = AnimationId::None;
    showUsesNamedAnimation = false;
    showNamedAnimation[0] = '\0';
    renderer.setAnimation(baseAnimationId, false);
}

void AnimationController::setBaseAnimation(AnimationId baseAnimation)
{
    if (baseAnimationId == baseAnimation)
        return;

    baseAnimationId = baseAnimation;
    dirtyAnimation = true;
}

AnimationId AnimationController::baseAnimation() const
{
    return baseAnimationId;
}

bool AnimationController::hasAnimation(AnimationId id) const
{
    return renderer.frameCountFor(id) > 0;
}

bool AnimationController::hasNamedAnimation(const char *name) const
{
    return renderer.frameCountForName(name) > 0;
}

bool AnimationController::hasAnimations(const AnimationId *ids, size_t count) const
{
    if (ids == nullptr)
        return false;

    for (size_t i = 0; i < count; ++i)
    {
        if (!hasAnimation(ids[i]))
            return false;
    }
    return true;
}

void AnimationController::queueAnimation(const Animation &animation)
{
    if (animation.usesNamedAnimation)
    {
        if (!hasNamedAnimation(animation.namedAnimation))
            return;
    }
    else if (animation.id != AnimationId::None && !hasAnimation(animation.id))
    {
        return;
    }

    auto insertIt = animationQueue.end();
    for (auto it = animationQueue.begin(); it != animationQueue.end(); ++it)
    {
        if (static_cast<uint8_t>(animation.priority) > static_cast<uint8_t>(it->priority))
        {
            insertIt = it;
            break;
        }
    }
    animationQueue.insert(insertIt, animation);
}

void AnimationController::queueNamedAnimation(const char *name,
                                              unsigned long durationMs,
                                              bool playOnce,
                                              AnimationOwner owner,
                                              AnimationPriority priority)
{
    if (name == nullptr || name[0] == '\0' || !hasNamedAnimation(name))
        return;

    Animation animation(AnimationId::None, durationMs, playOnce, owner, priority);
    animation.usesNamedAnimation = true;
    strncpy(animation.namedAnimation, name, sizeof(animation.namedAnimation) - 1);
    animation.namedAnimation[sizeof(animation.namedAnimation) - 1] = '\0';
    queueAnimation(animation);
}

void AnimationController::clearByOwner(AnimationOwner owner)
{
    for (auto it = animationQueue.begin(); it != animationQueue.end();)
    {
        if (it->owner == owner)
            it = animationQueue.erase(it);
        else
            ++it;
    }

    if (hasActiveAnimation && activeAnimation.owner == owner)
    {
        hasActiveAnimation = false;
        activeAnimation = Animation();
        displayDuration = 0;
        animateDone = true;
        showAnimationId = AnimationId::None;
        showUsesNamedAnimation = false;
        showNamedAnimation[0] = '\0';
    }
}

bool AnimationController::hasAnimationForOwner(AnimationOwner owner) const
{
    if (hasActiveAnimation && activeAnimation.owner == owner)
        return true;

    for (const Animation &animation : animationQueue)
    {
        if (animation.owner == owner)
            return true;
    }
    return false;
}

AnimationId AnimationController::currentCommandAnimationId() const
{
    if (hasActiveAnimation && activeAnimation.owner == AnimationOwner::Command)
        return activeAnimation.id;

    for (const Animation &animation : animationQueue)
    {
        if (animation.owner == AnimationOwner::Command)
            return animation.id;
    }

    return AnimationId::None;
}

void AnimationController::markDirty()
{
    dirtyAnimation = true;
}

void AnimationController::requestFullRedraw()
{
    dirtyAnimation = true;
    showAnimationId = AnimationId::None;
    showUsesNamedAnimation = false;
    showNamedAnimation[0] = '\0';
    animateDone = true;
    lastFrameTime = 0;
}

void AnimationController::showResourceError()
{
    renderer.showResourceError();
}

void AnimationController::showStatusNotFound()
{
    renderer.showStatusNotFound();
}

void AnimationController::updateElapsed(unsigned long elapsed)
{
    if (!hasActiveAnimation)
        return;

    displayDuration -= static_cast<long>(elapsed);
    if (displayDuration > 0 && activeAnimation.isFixedFrame())
        return;

    if (displayDuration > 0 && !animateDone)
        return;

    hasActiveAnimation = false;
    dirtyAnimation = true;
    animateDone = false;
}

void AnimationController::tryStartNextAnimation()
{
    if (hasActiveAnimation || animationQueue.empty())
        return;

    activeAnimation = animationQueue.front();
    animationQueue.pop_front();
    hasActiveAnimation = true;
    displayDuration = static_cast<long>(activeAnimation.durationMs);
}

void AnimationController::render(unsigned long now)
{
    const bool frameDue = (now - lastFrameTime >= frameInterval);
    if (frameDue)
        lastFrameTime = now;

    if (dirtyAnimation || (!showUsesNamedAnimation && showAnimationId == AnimationId::None))
    {
        bool playOnce = false;
        tryStartNextAnimation();
        if (hasActiveAnimation)
        {
            showUsesNamedAnimation = activeAnimation.usesNamedAnimation;
            if (showUsesNamedAnimation)
            {
                strncpy(showNamedAnimation, activeAnimation.namedAnimation, sizeof(showNamedAnimation) - 1);
                showNamedAnimation[sizeof(showNamedAnimation) - 1] = '\0';
                showAnimationId = AnimationId::None;
            }
            else
            {
                showNamedAnimation[0] = '\0';
                showAnimationId = activeAnimation.id;
            }
            playOnce = activeAnimation.playOnce;
        }
        else
        {
            showUsesNamedAnimation = false;
            showNamedAnimation[0] = '\0';
            showAnimationId = baseAnimationId;
        }

        frameInterval = showUsesNamedAnimation
                            ? renderer.frameIntervalForName(showNamedAnimation, frameIntervalSlow)
                            : renderer.frameIntervalFor(showAnimationId, frameIntervalSlow);
        if (hasActiveAnimation && activeAnimation.isFixedFrame())
        {
            animateDone = showUsesNamedAnimation
                              ? !renderer.ShowNamedAnimationFrame(showNamedAnimation, activeAnimation.frameIndex)
                              : !renderer.ShowAnimationFrame(showAnimationId, activeAnimation.frameIndex);
        }
        else
        {
            animateDone = showUsesNamedAnimation
                              ? !renderer.setNamedAnimation(showNamedAnimation, playOnce)
                              : !renderer.setAnimation(showAnimationId, playOnce);
            if (!animateDone)
                animateDone = renderer.advanceAnimationFrame();
        }
        dirtyAnimation = false;
    }
    else if (frameDue && !(hasActiveAnimation && activeAnimation.isFixedFrame()))
    {
        animateDone |= renderer.advanceAnimationFrame();
    }
}

void AnimationController::startBatteryAnimation()
{
    clearByOwner(AnimationOwner::Command);
    clearByOwner(AnimationOwner::Minigame);
    clearByOwner(AnimationOwner::System);
    showAnimationId = AnimationId::Battery;
    showUsesNamedAnimation = false;
    showNamedAnimation[0] = '\0';
    frameInterval = renderer.frameIntervalFor(showAnimationId, frameIntervalSlow);
    lastFrameTime = 0;
    animateDone = !renderer.setAnimation(showAnimationId, false);
    if (!animateDone)
        animateDone = renderer.advanceAnimationFrame();
}

void AnimationController::updateBatteryAnimation(unsigned long now)
{
    if (now - lastFrameTime < frameInterval)
        return;

    lastFrameTime = now;
    renderer.advanceAnimationFrame();
}

unsigned long AnimationController::defaultFrameInterval() const
{
    return frameIntervalSlow;
}

unsigned long AnimationController::frameIntervalFor(AnimationId id) const
{
    return renderer.frameIntervalFor(id, frameIntervalSlow);
}

unsigned long AnimationController::frameIntervalForName(const char *name) const
{
    return renderer.frameIntervalForName(name, frameIntervalSlow);
}

uint16_t AnimationController::frameCountFor(AnimationId id) const
{
    return renderer.frameCountFor(id);
}

uint16_t AnimationController::frameCountForName(const char *name) const
{
    return renderer.frameCountForName(name);
}

SdFat *AnimationController::sdCard() const
{
    return renderer.sdCard();
}
