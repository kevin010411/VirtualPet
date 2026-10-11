#pragma once
#include <stdint.h>

// Unsigned subtraction retains partial days across STOP and counter rollover.
class PetClock
{
public:
    void reset(uint32_t seconds)
    {
        boundary = target = seconds;
        paused = false;
    }
    void observe(uint32_t seconds)
    {
        if (!paused && !pending()) target = seconds;
    }
    void pause(uint32_t seconds)
    {
        if (paused) return;
        observe(seconds);
        pausedAt = seconds;
        paused = true;
    }
    void resume(uint32_t seconds)
    {
        if (!paused) return;
        const uint32_t elapsed = seconds - pausedAt;
        boundary += elapsed;
        target += elapsed;
        paused = false;
    }
    uint32_t due() const { return paused ? 0U : (target - boundary) / 2U; }
    bool pending() const { return due() != 0; }
    void consume() { if (pending()) boundary += 2U; }
private:
    uint32_t boundary = 0;
    uint32_t target = 0;
    uint32_t pausedAt = 0;
    bool paused = false;
};
