#include "common/CopyResourceName.h"

void copyResourceName(char *destination, size_t capacity, const char *resource)
{
    if (destination == nullptr || capacity == 0)
        return;
    size_t length = 0;
    while (length + 1 < capacity && resource[length] != '\0')
    {
        destination[length] = resource[length];
        ++length;
    }
    destination[length] = '\0';
}
