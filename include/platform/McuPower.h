#pragma once

#include <stdint.h>

enum class VoltageState : uint8_t;

namespace McuPower
{
void beginVoltageDetector();
VoltageState sampleVoltage();

// The predicate runs with interrupts masked, immediately before entering STOP.
void sleepIfIdle(bool (*shouldSleep)(uint32_t), uint32_t wakeGeneration);
}
