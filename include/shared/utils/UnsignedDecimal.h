#ifndef UNSIGNED_DECIMAL_H
#define UNSIGNED_DECIMAL_H

#include <stdint.h>

inline __attribute__((noinline)) uint32_t parseUnsignedDecimalUnchecked(const char *text)
{
    uint32_t parsed = 0;
    do
    {
        parsed = parsed * 10U + static_cast<uint32_t>(*text - '0');
        ++text;
    } while (*text != '\0');
    return parsed;
}

#endif
