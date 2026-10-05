#include "common/FirmwareRandom.h"

#include <assert.h>
#include <stdint.h>

int main()
{
    FirmwareRandom::seed(0x12345678);
    uint16_t first[16] = {};
    bool sawDifferent = false;
    for (uint8_t index = 0; index < 16; ++index)
    {
        first[index] = FirmwareRandom::below(5);
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
    return 0;
}
