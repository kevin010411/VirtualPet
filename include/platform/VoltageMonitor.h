#pragma once
#include <stdint.h>

enum class VoltageState : uint8_t
{
    Normal = 0,
    Dimmed = 1,
    Critical = 2
};

class VoltageMonitor
{
public:
    static constexpr uint32_t kSampleIntervalMs = 100;
    static constexpr uint32_t kRecoveryConfirmMs = 5000;
    static constexpr uint32_t kCriticalStopDelayMs = 5000;

    enum class WakeResult : uint8_t { Ready, Retry, ConfirmRecovery };

    VoltageState state() const { return current; }
    bool isCritical() const { return current == VoltageState::Critical; }
    bool sampleDue(uint32_t now) const { return now - lastSampleAt >= kSampleIntervalMs; }

    void begin(uint32_t now, VoltageState sample)
    {
        // The boot sample precedes periodic sampling and does not start timers.
        lastSampleAt = now;
        current = measured = sample;
    }

    void observe(uint32_t now, VoltageState sample)
    {
        lastSampleAt = now;
        measured = sample;
        if (measured == VoltageState::Critical)
        {
            if (criticalBelowSince == 0) criticalBelowSince = now;
        }
        else criticalBelowSince = 0;

        const uint8_t measuredRank = static_cast<uint8_t>(measured);
        const uint8_t currentRank = static_cast<uint8_t>(current);
        if (measuredRank > currentRank)
        {
            recoverySince = 0;
            current = measured;
        }
        else if (measuredRank < currentRank)
        {
            if (recoverySince == 0) recoverySince = now;
            else if (now - recoverySince >= kRecoveryConfirmMs)
            {
                current = static_cast<VoltageState>(currentRank - 1U);
                recoverySince = now;
            }
        }
        else recoverySince = 0;
    }

    bool shouldStop(uint32_t now) const
    {
        return isCritical() && measured == VoltageState::Critical &&
               criticalBelowSince != 0 && now - criticalBelowSince >= kCriticalStopDelayMs;
    }

    WakeResult observeWake(VoltageState sample)
    {
        measured = sample;
        if (sample == VoltageState::Critical)
        {
            current = VoltageState::Critical;
            recoverySince = 0;
            return WakeResult::Retry;
        }
        if (isCritical()) return WakeResult::ConfirmRecovery;
        if (static_cast<uint8_t>(sample) > static_cast<uint8_t>(current)) current = sample;
        return WakeResult::Ready;
    }

    void completeWakeRecovery(VoltageState sample, uint32_t now)
    {
        current = VoltageState::Dimmed;
        measured = sample;
        criticalBelowSince = 0;
        recoverySince = now;
    }

private:
    VoltageState current = VoltageState::Normal;
    VoltageState measured = VoltageState::Normal;
    uint32_t lastSampleAt = 0;
    uint32_t recoverySince = 0;
    uint32_t criticalBelowSince = 0;
};
