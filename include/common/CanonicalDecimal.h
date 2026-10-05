#ifndef CANONICAL_DECIMAL_H
#define CANONICAL_DECIMAL_H

#include <stdint.h>

namespace CanonicalDecimal
{
inline bool parseUnsigned(const char *text, uint32_t maximum, uint32_t &value,
                          bool allowZero = true)
{
    if (text == nullptr || text[0] == '\0' ||
        (text[0] == '0' && text[1] != '\0'))
        return false;
    uint32_t parsed = 0;
    for (const char *cursor = text; *cursor != '\0'; ++cursor)
    {
        if (*cursor < '0' || *cursor > '9')
            return false;
        const uint32_t digit = static_cast<uint32_t>(*cursor - '0');
        if (parsed > (maximum - digit) / 10U)
            return false;
        parsed = parsed * 10U + digit;
    }
    if (!allowZero && parsed == 0)
        return false;
    value = parsed;
    return true;
}
} // namespace CanonicalDecimal

#endif // CANONICAL_DECIMAL_H
