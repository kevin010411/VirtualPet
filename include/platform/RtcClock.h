#pragma once
#include <stdint.h>

namespace RtcClock
{
bool begin();
bool synchronize();
bool readSeconds(uint32_t &seconds);
// Nonblocking recovery. Epoch changes only when the backup counter is rebuilt.
bool pollSeconds(uint32_t &seconds, uint32_t &epoch);
void requestSynchronization();
}
