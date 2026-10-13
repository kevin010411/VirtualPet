#include "audio/SequenceAudio.h"
#if ENABLE_SEQUENCE_AUDIO
void SequenceAudioPlayer::prepare(SdFat *sd, const SequenceAudioRef &reference)
{
    stop();
    prepared_ = reference.bound() && SequenceAudioPlatform::prepare(sd, reference);
    if (reference.bound() && !prepared_ && firstError_ == SequenceAudioError::None)
        firstError_ = SequenceAudioError::ReadFailed;
}
void SequenceAudioPlayer::start(uint32_t now)
{
    timelineStarted_ = true;
    fallbackAt_ = startedAt_ = now;
    lastElapsed_ = 0;
    clockFallback_ = !prepared_ || !SequenceAudioPlatform::start();
    if (prepared_ && clockFallback_ && firstError_ == SequenceAudioError::None)
        firstError_ = SequenceAudioError::HardwareFailed;
}
uint32_t SequenceAudioPlayer::timelineMs(uint32_t now)
{
    SequenceAudioPlatform::service();
    if (!timelineStarted_) return 0;
    const auto failure = SequenceAudioPlatform::error();
    if (failure != SequenceAudioError::None && firstError_ == SequenceAudioError::None)
        firstError_ = failure;
    if (!clockFallback_)
    {
        const uint32_t elapsed = SequenceAudioPlatform::consumedSamples() / 8;
        if (elapsed > lastElapsed_) lastElapsed_ = elapsed;
        if (!SequenceAudioPlatform::active())
        {
            clockFallback_ = true;
            fallbackAt_ = now;
            const uint32_t fromOrigin = now - startedAt_;
            if (fromOrigin > lastElapsed_) lastElapsed_ = fromOrigin;
        }
        return lastElapsed_;
    }
    const uint32_t delta = now - fallbackAt_;
    fallbackAt_ = now;
    lastElapsed_ += delta;
    return lastElapsed_;
}
void SequenceAudioPlayer::stop(bool fade)
{
    SequenceAudioPlatform::stop(fade);
    prepared_ = false;
    timelineStarted_ = false;
}
bool SequenceAudioPlayer::audible() const { return SequenceAudioPlatform::active(); }
#endif
