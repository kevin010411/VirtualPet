#ifndef BASE_ANIMATION_ROTATION_H
#define BASE_ANIMATION_ROTATION_H

#include <Arduino.h>

class Renderer;

class BaseAnimationRotation
{
public:
    void reset();
    bool setBaseAnimation(const char *baseAnimation, const Renderer &renderer);
    bool onLoopCompletedAndRotateIfDue(const Renderer &renderer);
    const char *selectedAnimation() const;

private:
    static constexpr uint8_t kLoopsPerSelection = 10;
    static constexpr size_t kAnimationNameSize = 32;

    char baseAnimationName[kAnimationNameSize] = {};
    char selectedAnimationName[kAnimationNameSize] = {};
    uint8_t completedLoops = 0;

    bool selectVersion(const char *baseAnimation,
                       const Renderer &renderer,
                       char *destination,
                       size_t destinationSize) const;
};

#endif // BASE_ANIMATION_ROTATION_H
