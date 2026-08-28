#include "animation/application/AnimationController.h"

#include <limits.h>
#include <string.h>
#include "presentation/adapters/rendering/Renderer.h"

namespace
{
constexpr uint8_t kMaxRepeatedActionPlaybackCount = 5;
constexpr unsigned long kCompletePlaybackSafetyMs = 3000;
} // namespace

AnimationController::AnimationController(Renderer &rendererRef)
    : renderer(rendererRef)
{
}

void AnimationController::setup(AnimationId baseAnimation)
{
    resetPlaybackState();
    setBaseAnimation(baseAnimation);
    renderer.setAnimation(baseAnimationId, false);
}

void AnimationController::setup(const char *baseAnimation)
{
    resetPlaybackState();
    setBaseAnimation(baseAnimation);
    renderer.setNamedAnimation(baseRotation.selectedAnimation(), false);
}

void AnimationController::resetPlaybackState()
{
    renderer.initAnimations();
    animationQueueCount = 0;
    activeAnimation = Animation();
    hasActiveAnimation = false;
    activeRepeatsRemaining = 0;
    baseAnimationId = AnimationId::None;
    baseUsesNamedAnimation = false;
    baseRotation.reset();
    displayDuration = 0;
    dirtyAnimation = true;
    animateDone = true;
    frameInterval = frameIntervalSlow;
    lastFrameTime = 0;
    lastPlaybackUpdateTime = 0;
    showAnimationId = AnimationId::None;
    showUsesNamedAnimation = false;
    showNamedAnimation[0] = '\0';
    playbackFailedThisTick = false;
    playbackFailedAnimationIdThisTick = AnimationId::None;
}

void AnimationController::setBaseAnimation(AnimationId baseAnimation)
{
    if (!baseUsesNamedAnimation && baseAnimationId == baseAnimation)
        return;

    baseAnimationId = baseAnimation;
    baseUsesNamedAnimation = false;
    baseRotation.reset();
    dirtyAnimation = true;
}

void AnimationController::setBaseAnimation(const char *baseAnimation)
{
    if (!baseRotation.setBaseAnimation(baseAnimation, renderer))
        return;

    baseAnimationId = AnimationId::None;
    baseUsesNamedAnimation = true;
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

bool AnimationController::hasActionAnimation(AnimationId id) const
{
    if (id == AnimationId::None)
        return false;
    return hasActionAnimation(animationNameFromId(id));
}

bool AnimationController::hasActionAnimation(const char *baseName) const
{
    if (baseName == nullptr || baseName[0] == '\0')
        return false;
    const uint8_t variantCount = renderer.variantCountFor(baseName);
    if (variantCount > 0)
    {
        for (uint8_t index = 0; index < variantCount; ++index)
        {
            const char *variantName = renderer.variantNameFor(baseName, index);
            if (variantName == nullptr || !hasNamedAnimation(variantName))
                return false;
        }
        return true;
    }

    const AnimationId id = animationIdFromName(baseName);
    return id != AnimationId::None ? hasAnimation(id) : hasNamedAnimation(baseName);
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

PlaybackResult AnimationController::validate(const Animation &animation) const
{
    if (animation.repeatCount == 0 || animation.repeatCount > kMaxRepeatedActionPlaybackCount)
        return PlaybackResult::PlaybackFailed;

    uint16_t frameCount = 0;
    if (animation.usesNamedAnimation)
    {
        if (animation.namedAnimation[0] == '\0')
            return PlaybackResult::PlaybackFailed;
        frameCount = renderer.frameCountForName(animation.namedAnimation);
    }
    else
    {
        if (animation.id == AnimationId::None)
            return PlaybackResult::PlaybackFailed;
        frameCount = renderer.frameCountFor(animation.id);
    }

    if (frameCount == 0)
        return PlaybackResult::AnimationMissing;
    if (animation.isFixedFrame() && animation.frameIndex > frameCount)
        return PlaybackResult::PlaybackFailed;
    return PlaybackResult::Accepted;
}

PlaybackResult AnimationController::replace(const AnimationSequence &sequence)
{
    if (sequence.items == nullptr || sequence.count == 0)
        return PlaybackResult::PlaybackFailed;
    if (sequence.count > kMaxQueuedAnimations)
        return PlaybackResult::QueueFull;

    cancelAll();
    PlaybackResult firstFailure = PlaybackResult::Accepted;
    for (uint8_t index = 0; index < sequence.count; ++index)
    {
        const PlaybackResult result = validate(sequence.items[index]);
        if (result == PlaybackResult::Accepted)
            animationQueue[animationQueueCount++] = sequence.items[index];
        else if (firstFailure == PlaybackResult::Accepted)
            firstFailure = result;
    }

    if (animationQueueCount == 0)
        return firstFailure;
    return PlaybackResult::Accepted;
}

void AnimationController::cancelAll()
{
    animationQueueCount = 0;
    hasActiveAnimation = false;
    activeAnimation = Animation();
    activeRepeatsRemaining = 0;
    displayDuration = 0;
    dirtyAnimation = true;
    animateDone = true;
    showAnimationId = AnimationId::None;
    showUsesNamedAnimation = false;
    showNamedAnimation[0] = '\0';
    lastFrameTime = 0;
}

bool AnimationController::isBusy() const
{
    return hasActiveAnimation || animationQueueCount > 0;
}

AnimationId AnimationController::currentAnimationId() const
{
    if (hasActiveAnimation)
        return activeAnimation.id;
    return animationQueueCount > 0 ? animationQueue[0].id : AnimationId::None;
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

bool AnimationController::hasAnimationPending(AnimationId id) const
{
    if (hasActiveAnimation && activeAnimation.id == id)
        return true;
    for (uint8_t index = 0; index < animationQueueCount; ++index)
    {
        if (animationQueue[index].id == id)
            return true;
    }
    return false;
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

    completeActiveAnimation();
}

void AnimationController::completeActiveAnimation()
{
    if (!hasActiveAnimation)
        return;

    if (activeAnimation.repeatCount > 1 && activeAnimation.playOnce && activeRepeatsRemaining > 1)
    {
        --activeRepeatsRemaining;
        displayDuration = static_cast<long>(resolvedDuration(activeAnimation));
        dirtyAnimation = true;
        animateDone = false;
        return;
    }

    hasActiveAnimation = false;
    activeRepeatsRemaining = 0;
    dirtyAnimation = true;
    animateDone = false;
}

void AnimationController::tryStartNextAnimation()
{
    if (hasActiveAnimation || animationQueueCount == 0)
        return;

    activeAnimation = animationQueue[0];
    for (uint8_t index = 1; index < animationQueueCount; ++index)
        animationQueue[index - 1] = animationQueue[index];
    --animationQueueCount;
    hasActiveAnimation = true;
    activeRepeatsRemaining = activeAnimation.repeatCount == 0 ? 1 : activeAnimation.repeatCount;
    displayDuration = static_cast<long>(resolvedDuration(activeAnimation));
}

unsigned long AnimationController::resolvedDuration(const Animation &animation) const
{
    if (!animation.usesAutomaticDuration())
        return animation.durationMs;

    const uint16_t frameCount = animation.usesNamedAnimation
                                    ? renderer.frameCountForName(animation.namedAnimation)
                                    : renderer.frameCountFor(animation.id);
    const unsigned long frameIntervalMs = animation.usesNamedAnimation
                                              ? renderer.frameIntervalForName(animation.namedAnimation, frameIntervalSlow)
                                              : renderer.frameIntervalFor(animation.id, frameIntervalSlow);
    return completePlaybackDuration(frameCount, frameIntervalMs);
}

unsigned long AnimationController::completePlaybackDuration(uint16_t frameCount, unsigned long frameIntervalMs) const
{
    const unsigned long frameTransitions = static_cast<unsigned long>(frameCount) + 1;
    const unsigned long maxDisplayDuration = static_cast<unsigned long>(LONG_MAX);
    if (frameIntervalMs > (maxDisplayDuration - kCompletePlaybackSafetyMs) / frameTransitions)
        return maxDisplayDuration;

    return frameIntervalMs * frameTransitions + kCompletePlaybackSafetyMs;
}

PlaybackTickResult AnimationController::tick(unsigned long now)
{
    playbackFailedThisTick = false;
    playbackFailedAnimationIdThisTick = AnimationId::None;
    if (lastPlaybackUpdateTime == 0)
        lastPlaybackUpdateTime = now;

    const unsigned long elapsed = now - lastPlaybackUpdateTime;
    lastPlaybackUpdateTime = now;
    updateElapsed(elapsed);
    render(now);

    // A one-shot must hand its display ownership back immediately after its
    // final frame. Game's low-frequency Pet State tick must not delay idle.
    if (hasActiveAnimation && activeAnimation.playOnce &&
        !activeAnimation.isFixedFrame() && animateDone)
    {
        completeActiveAnimation();
        render(now);
    }
    return {
        playbackFailedThisTick ? PlaybackResult::PlaybackFailed : PlaybackResult::Accepted,
        playbackFailedAnimationIdThisTick,
    };
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
            showUsesNamedAnimation = baseUsesNamedAnimation;
            if (showUsesNamedAnimation)
            {
                strncpy(showNamedAnimation, baseRotation.selectedAnimation(), sizeof(showNamedAnimation) - 1);
                showNamedAnimation[sizeof(showNamedAnimation) - 1] = '\0';
                showAnimationId = AnimationId::None;
            }
            else
            {
                showNamedAnimation[0] = '\0';
                showAnimationId = baseAnimationId;
            }
        }

        frameInterval = showUsesNamedAnimation
                            ? renderer.frameIntervalForName(showNamedAnimation, frameIntervalSlow)
                            : renderer.frameIntervalFor(showAnimationId, frameIntervalSlow);
        if (hasActiveAnimation && activeAnimation.isFixedFrame())
        {
            const bool rendered = showUsesNamedAnimation
                                      ? renderer.ShowNamedAnimationFrame(showNamedAnimation, activeAnimation.frameIndex)
                                      : renderer.ShowAnimationFrame(showAnimationId, activeAnimation.frameIndex);
            animateDone = !rendered;
            if (!rendered)
            {
                playbackFailedAnimationIdThisTick = activeAnimation.id;
                playbackFailedThisTick = true;
            }
        }
        else
        {
            animateDone = showUsesNamedAnimation
                              ? !renderer.setNamedAnimation(showNamedAnimation, playOnce)
                              : !renderer.setAnimation(showAnimationId, playOnce);
            if (!animateDone)
            {
                animateDone = renderer.advanceAnimationFrame();
                if (renderer.animationFrameFailed() && hasActiveAnimation)
                {
                    playbackFailedAnimationIdThisTick = activeAnimation.id;
                    playbackFailedThisTick = true;
                }
            }
        }
        dirtyAnimation = false;
    }
    else if (frameDue &&
             !(hasActiveAnimation && activeAnimation.isFixedFrame()) &&
             !(hasActiveAnimation && activeAnimation.playOnce && animateDone))
    {
        if (!hasActiveAnimation && showUsesNamedAnimation &&
            renderer.willRestartAnimationLoop() && baseRotation.onLoopCompletedAndRotateIfDue(renderer))
        {
            strncpy(showNamedAnimation, baseRotation.selectedAnimation(), sizeof(showNamedAnimation) - 1);
            showNamedAnimation[sizeof(showNamedAnimation) - 1] = '\0';
            animateDone = !renderer.setNamedAnimation(showNamedAnimation, false);
        }
        animateDone |= renderer.advanceAnimationFrame();
        if (renderer.animationFrameFailed() && hasActiveAnimation)
        {
            playbackFailedAnimationIdThisTick = activeAnimation.id;
            playbackFailedThisTick = true;
        }
    }
}

void AnimationController::startBatteryAnimation()
{
    cancelAll();
    showAnimationId = AnimationId::Battery;
    showUsesNamedAnimation = false;
    showNamedAnimation[0] = '\0';
    frameInterval = renderer.frameIntervalFor(showAnimationId, frameIntervalSlow);
    lastFrameTime = 0;
    animateDone = !renderer.setAnimation(showAnimationId, false);
    if (!animateDone)
        animateDone = renderer.advanceAnimationFrame();
    dirtyAnimation = false;
}

void AnimationController::updateBatteryAnimation(unsigned long now)
{
    if (now - lastFrameTime < frameInterval)
        return;

    lastFrameTime = now;
    renderer.advanceAnimationFrame();
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
