#ifndef STATUS_SET_SELECTION_H
#define STATUS_SET_SELECTION_H

#include <stdint.h>

using StatusSetIndexSource = uint8_t (*)(uint8_t setCount);

bool selectStatusSetIndex(
    uint8_t setCount,
    StatusSetIndexSource indexSource,
    uint8_t &selectedIndex);

#endif // STATUS_SET_SELECTION_H
