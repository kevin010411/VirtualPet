#include "common/FirmwareRandom.h"

namespace
{
uint32_t state = 1;
}

void FirmwareRandom::seed(uint32_t value)
{
    if (value != 0)
        state = value;
}

uint16_t FirmwareRandom::below(uint16_t upperExclusive)
{
    if (upperExclusive == 0)
        return 0;

    // xorshift32: one shared state, no heap or libc rand/stdio dependency.
    uint32_t value = state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    state = value;
    return static_cast<uint16_t>(value % upperExclusive);
}
