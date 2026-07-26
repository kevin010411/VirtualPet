#include "commands/domain/StatusSetSelection.h"

bool selectStatusSetIndex(
    uint8_t setCount,
    StatusSetIndexSource indexSource,
    uint8_t &selectedIndex)
{
    if (setCount == 0 || indexSource == nullptr)
        return false;

    selectedIndex = indexSource(setCount);
    return selectedIndex < setCount;
}
