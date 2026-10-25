#include "common/FirmwareRandom.h"

#include <assert.h>
#include <stdint.h>

int main()
{
    FirmwareRandom::seed(0x12345678);
    uint16_t first[16] = {};
    uint32_t reference = 0x12345678;
    bool sawDifferent = false;
    for (uint8_t index = 0; index < 16; ++index)
    {
        first[index] = FirmwareRandom::below(5);
        reference ^= reference << 13;
        reference ^= reference >> 17;
        reference ^= reference << 5;
        assert(first[index] == reference % 5);
        assert(first[index] < 5);
        sawDifferent |= first[index] != first[0];
    }
    assert(sawDifferent);

    FirmwareRandom::seed(0x12345678);
    for (uint8_t index = 0; index < 16; ++index)
        assert(FirmwareRandom::below(5) == first[index]);

    FirmwareRandom::seed(0x12345678);
    assert(FirmwareRandom::below(0) == 0);
    assert(FirmwareRandom::below(5) == first[0]);

    FirmwareRandom::seed(0x12345678);
    FirmwareRandom::seed(0);
    assert(FirmwareRandom::below(5) == first[0]);
    assert(FirmwareRandom::below(1) == 0);
    FirmwareRandom::seed(0x12345678);
    uint32_t wide[16] = {};
    bool exceeded16Bits = false;
    for (uint8_t index = 0; index < 16; ++index)
    {
        wide[index] = FirmwareRandom::belowWide(196605);
        assert(wide[index] < 196605);
        exceeded16Bits |= wide[index] > UINT16_MAX;
    }
    assert(exceeded16Bits);
    FirmwareRandom::seed(0x12345678);
    assert(FirmwareRandom::belowWide(0) == 0);
    for (uint8_t index = 0; index < 16; ++index)
        assert(FirmwareRandom::belowWide(196605) == wide[index]);
    assert(FirmwareRandom::belowWide(1) == 0);
    // This seed's first xorshift value is UINT32_MAX, in the rejected tail for
    // 196605; the next value is 253983, so rejection must produce 57378.
    FirmwareRandom::seed(0x5e6cfce7);
    assert(FirmwareRandom::belowWide(196605) == 57378);
    return 0;
}
