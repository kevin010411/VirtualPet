#ifndef ANIMATION_H
#define ANIMATION_H

#include <Arduino.h>

struct Animation
{
    String name;
    unsigned long duration;
    bool showOnce;
    Animation(String n, unsigned long d, bool once = false) : name(n), duration(d), showOnce(once) {}
};

#endif