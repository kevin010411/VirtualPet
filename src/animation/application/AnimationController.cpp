#include "animation/application/AnimationController.h"

#include <limits.h>
#include <string.h>
#include "presentation/adapters/rendering/Renderer.h"
#include "pet_behavior/domain/PetBehaviorTypes.h"

namespace
{
constexpr uint8_t kMaxRepeatedActionPlaybackCount = 5;
constexpr unsigned long kCompletePlaybackSafetyMs = 3000;
} // namespace


AnimationController::AnimationController(Renderer &rendererRef) : renderer(rendererRef) {}

void AnimationController::configureRuntimeContract(const PetBehaviorConfig &config)
{
    runtimeContract = &config;
}

AssetData::AnimationRef AnimationController::systemAnimation(FirmwarePlaybackRole id) const
{
    const size_t index = static_cast<size_t>(id);
    return runtimeContract != nullptr && index < kFirmwarePlaybackRoleCount
               ? runtimeContract->systemAnimations[index]
               : AssetData::AnimationRef{};
}

AssetData::AnimationRef AnimationController::resolvedAnimation(const Animation &animation) const
{
    return animation.asset.valid() ? animation.asset : systemAnimation(animation.playbackRole);
}

void AnimationController::setup(const AssetData::AnimationRef &baseAnimation)
{
    resetPlaybackState();
    setBaseAnimation(baseAnimation);
    renderer.setAnimation(baseRotation.selectedAnimation(), baseRotation.selectedVersion(), false);
}

void AnimationController::resetPlaybackState()
{
    animationQueueCount = 0;
    activeAnimation = {};
    hasActiveAnimation = false;
    activeRepeatsRemaining = 0;
    baseAnimationRef = {};
    baseRotation.reset();
    displayDuration = 0;
    dirtyAnimation = true;
    animateDone = true;
    frameInterval = frameIntervalSlow;
    lastFrameTime = 0;
    lastPlaybackUpdateTime = 0;
    showPlaybackRole = FirmwarePlaybackRole::None;
    showAnimation = {};
    showVersionIndex = 0;
    playbackFailedThisTick = false;
    playbackFailedRoleThisTick = FirmwarePlaybackRole::None;
    renderer.initAnimations();
}

void AnimationController::setBaseAnimation(const AssetData::AnimationRef &baseAnimation)
{
    if (baseRotation.setBaseAnimation(baseAnimation, renderer))
    {
        baseAnimationRef = baseAnimation;
        dirtyAnimation = true;
    }
}

AssetData::AnimationRef AnimationController::baseAnimation() const
{
    return baseAnimationRef;
}

bool AnimationController::hasAnimation(FirmwarePlaybackRole id) const
{
    return hasAnimation(systemAnimation(id));
}

bool AnimationController::hasAnimation(const AssetData::AnimationRef &animation) const
{
    return animation.valid() && renderer.frameCountFor(animation) > 0;
}

bool AnimationController::hasActionAnimation(FirmwarePlaybackRole id) const
{
    return hasAnimation(id);
}

bool AnimationController::hasAnimations(const FirmwarePlaybackRole *ids, size_t count) const
{
    if (ids == nullptr || count == 0)
        return false;
    for (size_t index = 0; index < count; ++index)
    {
        if (!hasAnimation(ids[index]))
            return false;
    }
    return true;
}

PlaybackResult AnimationController::validate(const Animation &animation) const
{
    if (animation.repeatCount == 0 || animation.repeatCount > kMaxRepeatedActionPlaybackCount)
        return PlaybackResult::PlaybackFailed;
    const AssetData::AnimationRef reference = resolvedAnimation(animation);
    if (!reference.valid())
        return PlaybackResult::AnimationMissing;
    const uint16_t frameCount = renderer.frameCountFor(reference, animation.versionIndex);
    if (frameCount == 0 || (animation.isFixedFrame() && animation.frameIndex > frameCount))
        return PlaybackResult::AnimationMissing;
    return PlaybackResult::Accepted;
}

PlaybackResult AnimationController::replace(const AnimationSequence &sequence)
{
    if (sequence.items == nullptr || sequence.count == 0 || sequence.count > kMaxQueuedAnimations)
        return PlaybackResult::QueueFull;
    for (uint8_t index = 0; index < sequence.count; ++index)
    {
        const PlaybackResult result = validate(sequence.items[index]);
        if (result != PlaybackResult::Accepted)
            return result;
    }
    animationQueueCount = sequence.count;
    for (uint8_t index = 0; index < sequence.count; ++index)
        animationQueue[index] = sequence.items[index];
    hasActiveAnimation = false;
    activeRepeatsRemaining = 0;
    dirtyAnimation = true;
    animateDone = false;
    return PlaybackResult::Accepted;
}

void AnimationController::cancelAll()
{
    animationQueueCount = 0;
    hasActiveAnimation = false;
    activeRepeatsRemaining = 0;
    dirtyAnimation = true;
    animateDone = false;
}

bool AnimationController::isBusy() const
{
    return hasActiveAnimation || animationQueueCount > 0;
}

FirmwarePlaybackRole AnimationController::currentPlaybackRole() const
{
    return hasActiveAnimation ? activeAnimation.playbackRole : showPlaybackRole;
}

void AnimationController::requestFullRedraw()
{
    dirtyAnimation = true;
    showPlaybackRole = FirmwarePlaybackRole::None;
    showAnimation = {};
    animateDone = true;
    lastFrameTime = 0;
}

bool AnimationController::hasAnimationPending(FirmwarePlaybackRole id) const
{
    if (hasActiveAnimation && activeAnimation.playbackRole == id)
        return true;
    for (uint8_t index = 0; index < animationQueueCount; ++index)
    {
        if (animationQueue[index].playbackRole == id)
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
    if (activeAnimation.repeatCount > 1 && activeAnimation.playOnce &&
        activeRepeatsRemaining > 1)
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
    activeRepeatsRemaining = activeAnimation.repeatCount;
    displayDuration = static_cast<long>(resolvedDuration(activeAnimation));
}

unsigned long AnimationController::resolvedDuration(const Animation &animation) const
{
    if (!animation.usesAutomaticDuration())
        return animation.durationMs;
    const AssetData::AnimationRef reference = resolvedAnimation(animation);
    return completePlaybackDuration(
        renderer.frameCountFor(reference, animation.versionIndex),
        renderer.frameIntervalFor(
            reference, animation.versionIndex, frameIntervalSlow));
}

unsigned long AnimationController::completePlaybackDuration(
    uint16_t frameCount, unsigned long frameIntervalMs) const
{
    const unsigned long transitions = static_cast<unsigned long>(frameCount) + 1UL;
    const unsigned long maximum = static_cast<unsigned long>(LONG_MAX);
    if (frameIntervalMs > (maximum - kCompletePlaybackSafetyMs) / transitions)
        return maximum;
    return frameIntervalMs * transitions + kCompletePlaybackSafetyMs;
}

PlaybackTickResult AnimationController::tick(unsigned long now)
{
    playbackFailedThisTick = false;
    playbackFailedRoleThisTick = FirmwarePlaybackRole::None;
    if (lastPlaybackUpdateTime == 0)
        lastPlaybackUpdateTime = now;
    const unsigned long elapsed = now - lastPlaybackUpdateTime;
    lastPlaybackUpdateTime = now;
    updateElapsed(elapsed);
    render(now);
    if (hasActiveAnimation && activeAnimation.playOnce &&
        !activeAnimation.isFixedFrame() && animateDone)
    {
        completeActiveAnimation();
        render(now);
    }
    return {
        playbackFailedThisTick ? PlaybackResult::PlaybackFailed
                               : PlaybackResult::Accepted,
        playbackFailedRoleThisTick,
    };
}

void AnimationController::render(unsigned long now)
{
    const bool frameDue = now - lastFrameTime >= frameInterval;
    if (frameDue)
        lastFrameTime = now;

    if (dirtyAnimation || !showAnimation.valid())
    {
        bool playOnce = false;
        tryStartNextAnimation();
        if (hasActiveAnimation)
        {
            showAnimation = resolvedAnimation(activeAnimation);
            showVersionIndex = activeAnimation.versionIndex;
            showPlaybackRole = activeAnimation.playbackRole;
            playOnce = activeAnimation.playOnce;
        }
        else
        {
            showAnimation = baseRotation.selectedAnimation();
            showVersionIndex = baseRotation.selectedVersion();
            showPlaybackRole = FirmwarePlaybackRole::None;
        }

        frameInterval = renderer.frameIntervalFor(
            showAnimation, showVersionIndex, frameIntervalSlow);
        if (hasActiveAnimation && activeAnimation.isFixedFrame())
        {
            const bool rendered = renderer.ShowAnimationFrame(
                showAnimation, showVersionIndex, activeAnimation.frameIndex);
            animateDone = !rendered;
            if (!rendered)
            {
                playbackFailedRoleThisTick = activeAnimation.playbackRole;
                playbackFailedThisTick = true;
            }
        }
        else
        {
            animateDone = !renderer.setAnimation(
                showAnimation, showVersionIndex, playOnce);
            if (!animateDone)
            {
                animateDone = renderer.advanceAnimationFrame();
                if (renderer.animationFrameFailed() && hasActiveAnimation)
                {
                    playbackFailedRoleThisTick = activeAnimation.playbackRole;
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
        if (!hasActiveAnimation && renderer.willRestartAnimationLoop() &&
            baseRotation.onLoopCompletedAndRotateIfDue(renderer))
        {
            showVersionIndex = baseRotation.selectedVersion();
            animateDone = !renderer.setAnimation(
                showAnimation, showVersionIndex, false);
        }
        animateDone |= renderer.advanceAnimationFrame();
        if (renderer.animationFrameFailed() && hasActiveAnimation)
        {
            playbackFailedRoleThisTick = activeAnimation.playbackRole;
            playbackFailedThisTick = true;
        }
    }
}

void AnimationController::startBatteryAnimation()
{
    cancelAll();
    showPlaybackRole = FirmwarePlaybackRole::Battery;
    showAnimation = systemAnimation(FirmwarePlaybackRole::Battery);
    showVersionIndex = 0;
    frameInterval = renderer.frameIntervalFor(
        showAnimation, showVersionIndex, frameIntervalSlow);
    lastFrameTime = 0;
    animateDone = !renderer.setAnimation(showAnimation, showVersionIndex, false);
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

unsigned long AnimationController::frameIntervalFor(FirmwarePlaybackRole id) const
{
    return renderer.frameIntervalFor(systemAnimation(id), 0, frameIntervalSlow);
}

uint16_t AnimationController::frameCountFor(FirmwarePlaybackRole id) const
{
    return frameCountFor(systemAnimation(id));
}

uint16_t AnimationController::frameCountFor(
    const AssetData::AnimationRef &animation) const
{
    return renderer.frameCountFor(animation);
}

SdFat *AnimationController::sdCard() const
{
    return renderer.sdCard();
}
