#pragma once
#include <stddef.h>
#include <stdint.h>
extern unsigned long hostMillis;
inline unsigned long millis() { return hostMillis; }
