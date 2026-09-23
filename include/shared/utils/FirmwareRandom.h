#ifndef FIRMWARE_RANDOM_H
#define FIRMWARE_RANDOM_H

#include <stdint.h>

// Shared, allocation-free randomness for firmware choices. A zero seed leaves
// the current sequence unchanged, matching Arduino randomSeed(0).
namespace FirmwareRandom
{
void seed(uint32_t value);
uint16_t below(uint16_t upperExclusive);
}

#endif // FIRMWARE_RANDOM_H
