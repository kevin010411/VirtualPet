#include <assert.h>
#include <stdint.h>

#include "commands/domain/StatusSetSelection.h"

namespace
{
uint8_t requestedSetCount = 0;
uint8_t deterministicIndex = 0;

uint8_t deterministicStatusSetIndex(uint8_t setCount)
{
    requestedSetCount = setCount;
    return deterministicIndex;
}

void assertSelectedIndex(uint8_t setCount, uint8_t randomIndex, uint8_t expectedIndex)
{
    requestedSetCount = 0;
    deterministicIndex = randomIndex;
    uint8_t selectedIndex = 255;

    assert(selectStatusSetIndex(setCount, deterministicStatusSetIndex, selectedIndex));
    assert(requestedSetCount == setCount);
    assert(selectedIndex == expectedIndex);
}
} // namespace

int main()
{
    assertSelectedIndex(1, 0, 0);

    assertSelectedIndex(2, 0, 0);
    assertSelectedIndex(2, 1, 1);

    assertSelectedIndex(3, 0, 0);
    assertSelectedIndex(3, 1, 1);
    assertSelectedIndex(3, 2, 2);

    return 0;
}
