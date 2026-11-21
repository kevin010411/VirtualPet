#pragma once
#include <stdint.h>

struct DisplayRect
{
    uint8_t x, y, w, h;

    int right() const { return x + w; }
    int bottom() const { return y + h; }
    bool overlaps(DisplayRect other) const
    {
        return w && h && other.w && other.h && x < other.right() && other.x < right() &&
               y < other.bottom() && other.y < bottom();
    }
};
