#ifndef TEXT_BUFFER_H
#define TEXT_BUFFER_H

#include <stddef.h>
#include <stdint.h>

// Small, allocation-free alternative to printf-style formatting for firmware paths
// and identifiers. It always leaves a valid NUL-terminated string when capacity > 0.
class TextBuffer
{
public:
    TextBuffer(char *target, size_t capacity)
        : target(target), capacity(capacity)
    {
        if (target == nullptr || capacity == 0)
        {
            valid = false;
            return;
        }
        target[0] = '\0';
    }

    bool append(const char *text)
    {
        if (text == nullptr)
            return fail();

        while (*text != '\0')
        {
            if (!appendChar(*text++))
                return false;
        }
        return true;
    }

    bool append(const char *text, size_t count)
    {
        if (text == nullptr)
            return fail();

        for (size_t index = 0; index < count; ++index)
        {
            if (!appendChar(text[index]))
                return false;
        }
        return true;
    }

    bool appendUnsigned(uint32_t value)
    {
        char digits[10];
        size_t count = 0;
        do
        {
            digits[count++] = static_cast<char>('0' + (value % 10));
            value /= 10;
        } while (value != 0);

        while (count > 0)
        {
            if (!appendChar(digits[--count]))
                return false;
        }
        return true;
    }

    bool ok() const
    {
        return valid;
    }

private:
    bool appendChar(char value)
    {
        if (!valid || length + 1 >= capacity)
            return fail();

        target[length++] = value;
        target[length] = '\0';
        return true;
    }

    bool fail()
    {
        valid = false;
        if (target != nullptr && capacity > 0)
            target[length < capacity ? length : capacity - 1] = '\0';
        return false;
    }

    char *target = nullptr;
    size_t capacity = 0;
    size_t length = 0;
    bool valid = true;
};

#endif // TEXT_BUFFER_H
