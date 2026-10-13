#pragma once
#include <stdint.h>
#include "common/AppProfile.h"

struct SequenceAudioRef
{
    uint32_t offset = 0;
    uint32_t sampleCount = 0;
    bool bound() const { return sampleCount != 0; }
};

#if ENABLE_SEQUENCE_AUDIO
#include <SdFat.h>
enum class SequenceAudioError : uint8_t { None, ReadFailed, Underrun, HardwareFailed };
// The platform implementation owns bounded streaming buffers. Test platforms
// implement this same execution seam; no media validator lives in firmware.
namespace SequenceAudioPlatform
{
bool prepare(SdFat *sd, const SequenceAudioRef &reference);
bool start();
void service();
uint32_t consumedSamples();
bool active();
SequenceAudioError error();
void stop(bool fade);
}

class SequenceAudioPlayer
{
public:
    void prepare(SdFat *sd, const SequenceAudioRef &reference);
    void start(uint32_t now);
    uint32_t timelineMs(uint32_t now);
    void stop(bool fade = false);
    bool audible() const;
    bool ownsTimeline() const { return timelineStarted_; }
    SequenceAudioError firstError() const { return firstError_; }
private:
    bool prepared_ = false;
    bool timelineStarted_ = false;
    bool clockFallback_ = false;
    uint32_t fallbackAt_ = 0;
    uint32_t startedAt_ = 0;
    uint32_t lastElapsed_ = 0;
    SequenceAudioError firstError_ = SequenceAudioError::None;
};
#endif
