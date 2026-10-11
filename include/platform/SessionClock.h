#pragma once
#include <stdint.h>

// A monotonic session clock: RTC owns STOP elapsed time. While RTC is unavailable,
// millis owns awake elapsed time and STOP must be disabled by the caller.
class SessionClock
{
public:
    uint32_t update(uint32_t nowMs, bool rtcReady, uint32_t rtcSeconds, uint32_t epoch)
    {
        if (!started)
        {
            started = true;
            awakeAnchor = nowMs;
        }
        if (rtcReady)
        {
            if (haveRtc && epoch == rtcEpoch)
            {
                const uint32_t elapsed = rtcSeconds - lastRtc;
                // Reject a backward jump; a counter rebuild is identified by epoch.
                if (elapsed < 0x80000000UL && elapsed > countedAwake)
                    seconds += elapsed - countedAwake;
            }
            lastRtc = rtcSeconds;
            rtcEpoch = epoch;
            haveRtc = true;
            countedAwake = 0;
            awakeAnchor = nowMs;
        }
        else
        {
            const uint32_t elapsed = (nowMs - awakeAnchor) / 1000U;
            seconds += elapsed;
            countedAwake += elapsed;
            awakeAnchor += elapsed * 1000U;
        }
        return seconds;
    }
private:
    uint32_t seconds = 0, lastRtc = 0, rtcEpoch = 0;
    uint32_t awakeAnchor = 0, countedAwake = 0;
    bool started = false, haveRtc = false;
};
