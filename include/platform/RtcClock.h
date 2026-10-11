#pragma once
#include <stdint.h>

namespace RtcClock
{
bool begin();
bool synchronize();
bool readSeconds(uint32_t &seconds);
}
