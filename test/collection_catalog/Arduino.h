#ifndef GAME_STARTUP_ARDUINO_H
#define GAME_STARTUP_ARDUINO_H

#include <stddef.h>
#include <stdint.h>

extern unsigned long hostMillis;
inline unsigned long millis() { return hostMillis; }

template <typename T>
inline T max(T a, T b) { return a > b ? a : b; }

#endif
