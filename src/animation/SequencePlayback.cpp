#include "resources/AssetResources.h"
#include "animation/AnimationController.h"
#if ENABLE_SEQUENCE_AUDIO
#include "display/Renderer.h"

void AnimationController::prepareBasePlayback(unsigned long now)
{
    playbackFailedThisTick = false;
    playbackFailedRoleThisTick = FirmwarePlaybackRole::None;
    uint32_t elapsed = synchronizedBase && baseStarted ? audioPlayer.timelineMs(now) : 0;
    bool restart = !synchronizedBase;
    if (!restart && baseStarted && elapsed >= baseLoopDuration)
    {
        // Rotation keeps its existing ten complete loops per Version selection.
        // A late tick skips frames within this loop, not whole authored loops.
        baseRotation.onLoopCompletedAndRotateIfDue(resources);
        restart = true;
    }
    if (restart)
    {
        audioPlayer.stop();
        audioPlayer.prepare(resources.sdCard(), baseAudio);
        synchronizedBase = true;
        baseStarted = false;
        elapsed = 0;
        showAnimation = baseRotation.selectedAnimation();
        showVersionIndex = baseRotation.selectedVersion();
        showPlaybackRole = FirmwarePlaybackRole::None;
        frameInterval = resources.frameIntervalFor(showAnimation, showVersionIndex, frameIntervalSlow);
        if (frameInterval == 0) frameInterval = 1;
        baseLoopDuration = uint32_t(resources.frameCountFor(showAnimation, showVersionIndex)) * frameInterval;
        animateDone = !renderer.setAnimation(showAnimation, showVersionIndex, false);
        dirtyAnimation = false;
    }
    const uint16_t frame = uint16_t(elapsed / frameInterval + 1);
    const bool frameDue = restart || dirtyAnimation || frame != baseFrame;
    preparedFrame = frameDue ? PreparedFrame::Advance : PreparedFrame::Hold;
    baseFrame = frame;
    dirtyAnimation = false;
}

PlaybackTickResult AnimationController::tickBasePlayback()
{
    if (preparedFrame == PreparedFrame::Advance && !renderer.ShowAnimationFrame(showAnimation, showVersionIndex, baseFrame))
        playbackFailedThisTick = true;
    if (!baseStarted)
    {
        // Buffer preparation and the first TFT transfer precede the sample clock.
        // On SD failure the existing player preserves the visual millis timeline;
        // retry happens at a full animation loop, never every update.
        audioPlayer.start(millis());
        baseStarted = true;
    }
    preparedFrame = PreparedFrame::Unprepared;
    return {playbackFailedThisTick ? PlaybackResult::PlaybackFailed : PlaybackResult::Accepted,
            FirmwarePlaybackRole::None};
}

void AnimationController::prepareSynchronizedPlayback(unsigned long now)
{
    playbackFailedThisTick = false;
    playbackFailedRoleThisTick = FirmwarePlaybackRole::None;
    uint32_t elapsed = sequenceStarted ? audioPlayer.timelineMs(now) : 0;
    const uint32_t remaining = synchronizedDurationMs[synchronizedSegment] - synchronizedSegmentElapsedMs;
    const uint32_t passDuration = remaining < synchronizedPassMs[synchronizedSegment] ? remaining : synchronizedPassMs[synchronizedSegment];
    if (sequenceStarted && elapsed >= passDuration)
    {
        audioPlayer.stop(true);
        sequenceStarted = false;
        synchronizedSegmentElapsedMs += passDuration;
        if (synchronizedSegmentElapsedMs < synchronizedDurationMs[synchronizedSegment])
            ++synchronizedRepeat;
        else
        {
            synchronizedRepeat = 0;
            synchronizedSegmentElapsedMs = 0;
            ++synchronizedSegment;
        }
        if (synchronizedSegment >= animationQueueCount)
        {
            if (sequenceLoops) synchronizedSegment = 0;
            else
            {
                synchronizedSequence = false;
                animationQueueCount = 0;
                hasActiveAnimation = false;
                dirtyAnimation = true;
                preparedFrame = PreparedFrame::Unprepared;
                lastPlaybackUpdateTime = now;
                preparePlayback(now);
                return;
            }
        }
        synchronizedTargetPending = true;
        elapsed = 0;
    }
    activeAnimation = animationQueue[synchronizedSegment];
    hasActiveAnimation = true;
    showAnimation = resolvedAnimation(activeAnimation);
    showVersionIndex = activeAnimation.versionIndex;
    showPlaybackRole = activeAnimation.playbackRole;
    const uint32_t interval = synchronizedIntervalMs[synchronizedSegment];
    const uint16_t frame = activeAnimation.isFixedFrame() ? activeAnimation.frameIndex :
        uint16_t(elapsed / interval + 1);
    const bool targetChanged = synchronizedTargetPending;
    preparedFrame = targetChanged || frame != synchronizedFrame
        ? PreparedFrame::Advance : PreparedFrame::Hold;
    if (targetChanged)
    {
        audioPlayer.prepare(resources.sdCard(), audioFor(showAnimation));
        renderer.setAnimation(showAnimation, showVersionIndex, true);
        synchronizedTargetPending = false;
    }
    synchronizedFrame = frame;
}

PlaybackTickResult AnimationController::tickSynchronizedPlayback()
{
    if (preparedFrame == PreparedFrame::Advance && !renderer.ShowAnimationFrame(
            showAnimation, showVersionIndex, synchronizedFrame))
    {
        playbackFailedThisTick = true;
        playbackFailedRoleThisTick = showPlaybackRole;
    }
    if (!sequenceStarted)
    {
        // Display transfer completes before the sample clock begins. Render
        // failures remain visual failures; they do not roll back Action Effects.
        audioPlayer.start(millis());
        sequenceStarted = true;
    }
    preparedFrame = PreparedFrame::Unprepared;
    return {playbackFailedThisTick ? PlaybackResult::PlaybackFailed : PlaybackResult::Accepted,
            playbackFailedRoleThisTick};
}
#endif
