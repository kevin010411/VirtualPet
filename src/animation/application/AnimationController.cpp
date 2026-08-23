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
    failedAnimationId = AnimationId::None;
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

    uint8_t insertAt = animationQueueCount;
    for (uint8_t index = 0; index < animationQueueCount; ++index)
    {
        if (static_cast<uint8_t>(animation.priority) > static_cast<uint8_t>(animationQueue[index].priority))
        {
            insertAt = index;
            break;
        }
    }

    if (animationQueueCount == kMaxQueuedAnimations)
    {
        if (insertAt >= kMaxQueuedAnimations)
            return;
        --animationQueueCount;
    }

    for (uint8_t index = animationQueueCount; index > insertAt; --index)
        animationQueue[index] = animationQueue[index - 1];

    animationQueue[insertAt] = animation;
    ++animationQueueCount;
}

void AnimationController::queueNamedAnimation(const char *name,
                                              unsigned long durationMs,
                                              bool playOnce,
                                              AnimationOwner owner,
                                              AnimationPriority priority,
                                              uint16_t fixedFrameIndex)
{
    if (name == nullptr || name[0] == '\0' || !hasNamedAnimation(name))
        return;

    Animation animation(AnimationId::None, durationMs, playOnce, owner, priority, fixedFrameIndex);
    animation.usesNamedAnimation = true;
    strncpy(animation.namedAnimation, name, sizeof(animation.namedAnimation) - 1);
    animation.namedAnimation[sizeof(animation.namedAnimation) - 1] = '\0';
    queueAnimation(animation);
}

bool AnimationController::queueActionAnimation(AnimationId id,
                                                unsigned long durationMs,
                                                bool playOnce,
                                                AnimationOwner owner,
                                                AnimationPriority priority)
{
    if (id == AnimationId::None)
        return false;
    return queueActionAnimation(animationNameFromId(id), durationMs, playOnce, owner, priority, nullptr, 0);
}

bool AnimationController::queueActionAnimation(const char *baseName,
                                                unsigned long durationMs,
                                                bool playOnce,
                                                AnimationOwner owner,
                                                AnimationPriority priority)
{
    return queueActionAnimation(baseName, durationMs, playOnce, owner, priority, nullptr, 0);
}

bool AnimationController::queueActionAnimation(AnimationId id,
                                                unsigned long durationMs,
                                                bool playOnce,
                                                AnimationOwner owner,
                                                AnimationPriority priority,
                                                char *selectedName,
                                                size_t selectedNameSize)
{
    if (id == AnimationId::None)
        return false;
    return queueActionAnimation(animationNameFromId(id), durationMs, playOnce, owner, priority, selectedName, selectedNameSize);
}

bool AnimationController::queueActionAnimation(const char *baseName,
                                                unsigned long durationMs,
                                                bool playOnce,
                                                AnimationOwner owner,
                                                AnimationPriority priority,
                                                char *selectedName,
                                                size_t selectedNameSize)
{
    if (baseName == nullptr || baseName[0] == '\0')
        return false;

    const uint8_t variantCount = renderer.variantCountFor(baseName);
    if (variantCount > 0)
    {
        const char *variantName = renderer.variantNameFor(baseName, static_cast<uint8_t>(random(variantCount)));
        if (variantName == nullptr || !hasNamedAnimation(variantName))
            return false;
        queueNamedAnimation(variantName, durationMs, playOnce, owner, priority);
        if (selectedName != nullptr && selectedNameSize > 0)
        {
            strncpy(selectedName, variantName, selectedNameSize - 1);
            selectedName[selectedNameSize - 1] = '\0';
        }
        return true;
    }

    const AnimationId id = animationIdFromName(baseName);
    if (id != AnimationId::None)
    {
        if (!hasAnimation(id))
            return false;
        queueAnimation(Animation(id, durationMs, playOnce, owner, priority));
        if (selectedName != nullptr && selectedNameSize > 0)
        {
            strncpy(selectedName, baseName, selectedNameSize - 1);
            selectedName[selectedNameSize - 1] = '\0';
        }
        return true;
    }

    if (!hasNamedAnimation(baseName))
        return false;
    queueNamedAnimation(baseName, durationMs, playOnce, owner, priority);
    if (selectedName != nullptr && selectedNameSize > 0)
    {
        strncpy(selectedName, baseName, selectedNameSize - 1);
        selectedName[selectedNameSize - 1] = '\0';
    }
    return true;
}

bool AnimationController::queueCompleteAnimation(AnimationId id,
                                                 AnimationOwner owner,
                                                 AnimationPriority priority)
{
    if (!hasAnimation(id))
        return false;

    const uint16_t frameCount = renderer.frameCountFor(id);
    const unsigned long frameIntervalMs = renderer.frameIntervalFor(id, frameIntervalSlow);
    if (frameCount == 0 || frameIntervalMs == 0)
        return false;

    queueAnimation(Animation(id,
                             completePlaybackDuration(frameCount, frameIntervalMs),
                             true,
                             owner,
                             priority));
    return true;
}

bool AnimationController::queueRepeatedActionAnimation(const char *baseName,
                                                        uint8_t playbackCount,
                                                        AnimationOwner owner,
                                                        AnimationPriority priority,
                                                        char *selectedName,
                                                        size_t selectedNameSize)
{
    if (baseName == nullptr || baseName[0] == '\0' || playbackCount == 0 ||
        playbackCount > kMaxRepeatedActionPlaybackCount)
        return false;

    const char *selectedAnimation = baseName;
    AnimationId selectedId = AnimationId::None;
    bool usesNamedAnimation = false;
    uint16_t frameCount = 0;
    unsigned long frameIntervalMs = frameIntervalSlow;

    const uint8_t variantCount = renderer.variantCountFor(baseName);
    if (variantCount > 0)
    {
        selectedAnimation = renderer.variantNameFor(baseName, static_cast<uint8_t>(random(variantCount)));
        if (selectedAnimation == nullptr || !hasNamedAnimation(selectedAnimation))
            return false;

        usesNamedAnimation = true;
        frameCount = renderer.frameCountForName(selectedAnimation);
        frameIntervalMs = renderer.frameIntervalForName(selectedAnimation, frameIntervalSlow);
    }
    else
    {
        selectedId = animationIdFromName(baseName);
        if (selectedId != AnimationId::None)
        {
            if (!hasAnimation(selectedId))
                return false;

            frameCount = renderer.frameCountFor(selectedId);
            frameIntervalMs = renderer.frameIntervalFor(selectedId, frameIntervalSlow);
        }
        else
        {
            if (!hasNamedAnimation(baseName))
                return false;

            usesNamedAnimation = true;
            frameCount = renderer.frameCountForName(baseName);
            frameIntervalMs = renderer.frameIntervalForName(baseName, frameIntervalSlow);
        }
    }

    if (frameCount == 0 || frameIntervalMs == 0)
        return false;

    bool canQueue = animationQueueCount < kMaxQueuedAnimations;
    if (!canQueue)
    {
        for (uint8_t index = 0; index < animationQueueCount; ++index)
        {
            if (static_cast<uint8_t>(priority) > static_cast<uint8_t>(animationQueue[index].priority))
            {
                canQueue = true;
                break;
            }
        }
    }
    if (!canQueue)
        return false;

    Animation animation(selectedId,
                        completePlaybackDuration(frameCount, frameIntervalMs),
                        true,
                        owner,
                        priority);
    if (usesNamedAnimation && strlen(selectedAnimation) >= sizeof(animation.namedAnimation))
        return false;

    animation.repeatCount = playbackCount;
    animation.usesNamedAnimation = usesNamedAnimation;
    if (usesNamedAnimation)
    {
        strncpy(animation.namedAnimation, selectedAnimation, sizeof(animation.namedAnimation) - 1);
        animation.namedAnimation[sizeof(animation.namedAnimation) - 1] = '\0';
    }
    queueAnimation(animation);

    if (selectedName != nullptr && selectedNameSize > 0)
    {
        strncpy(selectedName, selectedAnimation, selectedNameSize - 1);
        selectedName[selectedNameSize - 1] = '\0';
    }
    return true;
}

void AnimationController::clearByOwner(AnimationOwner owner)
{
    for (uint8_t index = 0; index < animationQueueCount;)
    {
        if (animationQueue[index].owner == owner)
        {
            for (uint8_t move = index + 1; move < animationQueueCount; ++move)
                animationQueue[move - 1] = animationQueue[move];
            --animationQueueCount;
        }
        else
        {
            ++index;
        }
    }

    if (hasActiveAnimation && activeAnimation.owner == owner)
    {
        hasActiveAnimation = false;
        activeAnimation = Animation();
        activeRepeatsRemaining = 0;
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

    for (uint8_t index = 0; index < animationQueueCount; ++index)
    {
        if (animationQueue[index].owner == owner)
            return true;
    }
    return false;
}

AnimationId AnimationController::currentCommandAnimationId() const
{
    if (hasActiveAnimation && activeAnimation.owner == AnimationOwner::Command)
        return activeAnimation.id;

    for (uint8_t index = 0; index < animationQueueCount; ++index)
    {
        if (animationQueue[index].owner == AnimationOwner::Command)
            return animationQueue[index].id;
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

bool AnimationController::hasPlaybackFailure(AnimationId id) const
{
    return failedAnimationId == id;
}

void AnimationController::clearPlaybackFailure(AnimationId id)
{
    if (failedAnimationId == id)
        failedAnimationId = AnimationId::None;
}

void AnimationController::showActionAnimationError()
{
    renderer.showActionAnimationError();
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

    completeActiveAnimation();
}

void AnimationController::completeActiveAnimation()
{
    if (!hasActiveAnimation)
        return;

    if (activeAnimation.repeatCount > 1 && activeAnimation.playOnce && activeRepeatsRemaining > 1)
    {
        --activeRepeatsRemaining;
        displayDuration = static_cast<long>(activeAnimation.durationMs);
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
    displayDuration = static_cast<long>(activeAnimation.durationMs);
}

unsigned long AnimationController::completePlaybackDuration(uint16_t frameCount, unsigned long frameIntervalMs) const
{
    const unsigned long frameTransitions = static_cast<unsigned long>(frameCount) + 1;
    const unsigned long maxDisplayDuration = static_cast<unsigned long>(LONG_MAX);
    if (frameIntervalMs > (maxDisplayDuration - kCompletePlaybackSafetyMs) / frameTransitions)
        return maxDisplayDuration;

    return frameIntervalMs * frameTransitions + kCompletePlaybackSafetyMs;
}

void AnimationController::tick(unsigned long now)
{
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
            {
                animateDone = renderer.advanceAnimationFrame();
                if (renderer.animationFrameFailed() && hasActiveAnimation)
                    failedAnimationId = activeAnimation.id;
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
            failedAnimationId = activeAnimation.id;
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
