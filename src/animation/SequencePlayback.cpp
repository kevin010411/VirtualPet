#include "animation/AnimationController.h"
#if ENABLE_SEQUENCE_AUDIO
#include "display/Renderer.h"

void AnimationController::prepareSynchronizedPlayback(unsigned long now)
{
    playbackFailedThisTick = false;
    playbackFailedRoleThisTick = FirmwarePlaybackRole::None;
    uint32_t elapsed = sequenceStarted ? audioPlayer.timelineMs(now) : 0;
    if (sequenceDuration == 0 || (!sequenceLoops && elapsed >= sequenceDuration))
    {
        audioPlayer.stop(true);
        synchronizedSequence = sequenceStarted = false;
        animationQueueCount = 0;
        hasActiveAnimation = false;
        dirtyAnimation = true;
        playbackPrepared = false;
        lastPlaybackUpdateTime = now;
        preparePlayback(now);
        return;
    }
    if (sequenceLoops)
    {
        const uint32_t cycle = elapsed / sequenceDuration;
        if (cycle != lastSequenceCycle)
        {
            // A full loop restarts both tracks; the binding remains in runtime.
            audioPlayer.prepare(sdCard(), pendingSequenceAudio);
            sequenceStarted = false;
            synchronizedSegment = synchronizedRepeat = 255;
            lastSequenceCycle = 0;
            elapsed = 0;
        }
        else elapsed %= sequenceDuration;
    }
    uint8_t segment = 0;
    uint32_t segmentElapsed = elapsed;
    uint32_t interval = 1, onePass = 1;
    for (; segment < animationQueueCount; ++segment)
    {
        const auto &item = animationQueue[segment];
        interval = synchronizedIntervalMs[segment];
        onePass = synchronizedPassMs[segment];
        const uint32_t total = item.isFixedFrame() ? onePass : onePass * item.repeatCount;
        if (segmentElapsed < total) break;
        segmentElapsed -= total;
    }
    if (segment >= animationQueueCount) return;
    activeAnimation = animationQueue[segment];
    hasActiveAnimation = true;
    showAnimation = resolvedAnimation(activeAnimation);
    showVersionIndex = activeAnimation.versionIndex;
    showPlaybackRole = activeAnimation.playbackRole;
    const uint8_t repeat = activeAnimation.isFixedFrame() ? 0 : segmentElapsed / onePass;
    const uint16_t frame = activeAnimation.isFixedFrame() ? activeAnimation.frameIndex :
        uint16_t((segmentElapsed % onePass) / interval + 1);
    preparedTargetChanged = segment != synchronizedSegment || repeat != synchronizedRepeat;
    preparedFrameDue = preparedTargetChanged || frame != synchronizedFrame;
    if (preparedTargetChanged)
    {
        renderer.setAnimation(showAnimation, showVersionIndex, true);
        synchronizedSegment = segment;
        synchronizedRepeat = repeat;
    }
    synchronizedFrame = frame;
    playbackPrepared = true;
}

PlaybackTickResult AnimationController::tickSynchronizedPlayback()
{
    if (preparedFrameDue && !renderer.ShowAnimationFrame(
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
    playbackPrepared = false;
    return {playbackFailedThisTick ? PlaybackResult::PlaybackFailed : PlaybackResult::Accepted,
            playbackFailedRoleThisTick};
}
#endif
