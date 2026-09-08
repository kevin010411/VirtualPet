#ifndef RENDERER_STARTUP_ERROR_ARDUINO_H
#define RENDERER_STARTUP_ERROR_ARDUINO_H

#include <stddef.h>
#include <stdint.h>

template <typename T>
T max(T left, T right)
{
    return left > right ? left : right;
}

#endif
