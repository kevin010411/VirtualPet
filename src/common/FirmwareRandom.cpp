#include "common/FirmwareRandom.h"

namespace
{
uint32_t state = 1;

uint32_t next()
{
    uint32_t value = state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    state = value;
    return value;
}
}

void FirmwareRandom::seed(uint32_t value)
{
    if (value != 0)
        state = value;
}

uint16_t FirmwareRandom::below(uint16_t upperExclusive)
{
    return upperExclusive == 0 ? 0 : static_cast<uint16_t>(next() % upperExclusive);
}

uint32_t FirmwareRandom::belowWide(uint32_t upperExclusive)
{
    if (upperExclusive == 0)
        return 0;

    // xorshift32 visits 1..UINT32_MAX. Reject the incomplete tail so each
    // residue has the same number of source values, including wide bounds.
    const uint32_t limit = UINT32_MAX - UINT32_MAX % upperExclusive;
    uint32_t value;
    do
    {
        value = next();
    } while (value > limit);
    return value % upperExclusive;
}
